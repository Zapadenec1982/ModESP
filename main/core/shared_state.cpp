#include "shared_state.h"
#include <esp_log.h>
#include <algorithm>

namespace ModuChill {

static const char* TAG = "SharedState";

SharedState* SharedState::instance_ = nullptr;

SharedState::SharedState() 
    : subscription_counter_(1) {
    mutex_ = xSemaphoreCreateMutex();
    configASSERT(mutex_);
}

SharedState::~SharedState() {
    if (mutex_) {
        vSemaphoreDelete(mutex_);
    }
}

SharedState* SharedState::get_instance() {
    if (!instance_) {
        instance_ = new SharedState();
    }
    return instance_;
}

void SharedState::init() {
    ESP_LOGI(TAG, "SharedState initialized with %d max entries", MAX_ENTRIES);
}

void SharedState::deinit() {
    if (instance_) {
        delete instance_;
        instance_ = nullptr;
    }
}

bool SharedState::set_internal(const std::string& key, const StateValue& value) {
    if (key.length() > MAX_KEY_LENGTH) {
        ESP_LOGW(TAG, "Key too long: %s", key.c_str());
        return false;
    }
    
    // Check if string value is too long
    if (std::holds_alternative<std::string>(value)) {
        const auto& str = std::get<std::string>(value);
        if (str.length() > MAX_STRING_VALUE) {
            ESP_LOGW(TAG, "String value too long for key: %s", key.c_str());
            return false;
        }
    }
    
    // Store old value for comparison
    std::optional<StateValue> old_value;
    auto it = storage_.find(key);
    if (it != storage_.end()) {
        old_value = it->second.value;
    }
    
    // Update or create entry
    auto& entry = storage_[key];
    entry.value = value;
    entry.last_update = esp_timer_get_time() / 1000;
    entry.update_count++;
    
    // Check if value actually changed
    bool changed = !old_value.has_value() || old_value.value() != value;
    
    return changed;
}
void SharedState::trigger_subscriptions(const std::string& key) {
    std::vector<SubscriptionCallback> callbacks_to_call;
    
    // Collect matching callbacks (while holding mutex)
    for (const auto& [sub_key, sub_list] : subscriptions_) {
        if (key == sub_key || sub_key == "*" || 
            (sub_key.back() == '*' && key.substr(0, sub_key.length() - 1) == sub_key.substr(0, sub_key.length() - 1))) {
            for (const auto& sub : sub_list) {
                callbacks_to_call.push_back(sub.callback);
            }
        }
    }
    
    // Call callbacks outside mutex to avoid deadlock
    for (const auto& callback : callbacks_to_call) {
        try {
            callback(key);
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Exception in subscription callback for %s: %s", key.c_str(), e.what());
        } catch (...) {
            ESP_LOGE(TAG, "Unknown exception in subscription callback for %s", key.c_str());
        }
    }
}

std::optional<StateValue> SharedState::get_internal(const std::string& key) const {
    auto it = storage_.find(key);
    if (it != storage_.end()) {
        return it->second.value;
    }
    return std::nullopt;
}

bool SharedState::exists(const std::string& key) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    return state->storage_.find(key) != state->storage_.end();
}

bool SharedState::remove(const std::string& key) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    size_t removed = state->storage_.erase(key);
    if (removed > 0) {
        ESP_LOGD(TAG, "Removed key: %s", key.c_str());
        return true;
    }
    return false;
}

size_t SharedState::size() {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    return state->storage_.size();
}

void SharedState::clear() {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    state->storage_.clear();
    ESP_LOGI(TAG, "SharedState cleared");
}

std::vector<std::string> SharedState::get_keys(const std::string& pattern) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    std::vector<std::string> keys;
    
    for (const auto& [key, entry] : state->storage_) {
        if (pattern.empty() || key.find(pattern) != std::string::npos) {
            keys.push_back(key);
        }
    }
    
    return keys;
}

bool SharedState::has_changed(const std::string& pattern, uint32_t since_timestamp) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    for (const auto& [key, entry] : state->storage_) {
        if ((pattern.empty() || key.find(pattern) != std::string::npos) &&
            entry.last_update > since_timestamp) {
            return true;
        }
    }
    
    return false;
}

uint32_t SharedState::get_last_change_time(const std::string& key) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    auto it = state->storage_.find(key);
    if (it != state->storage_.end()) {
        return it->second.last_update;
    }
    return 0;
}

// Atomic operations
bool SharedState::compare_and_swap(const std::string& key, float expected, float new_value) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    auto current = state->get_internal(key);
    if (current.has_value() && std::holds_alternative<float>(*current)) {
        float current_float = std::get<float>(*current);
        if (std::abs(current_float - expected) < 0.0001f) { // Float comparison with epsilon
            state->set_internal(key, new_value);
            state->trigger_subscriptions(key);
            return true;
        }
    }
    return false;
}

float SharedState::increment(const std::string& key, float delta) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    float new_value = delta;
    auto current = state->get_internal(key);
    
    if (current.has_value() && std::holds_alternative<float>(*current)) {
        new_value = std::get<float>(*current) + delta;
    }
    
    state->set_internal(key, new_value);
    state->trigger_subscriptions(key);
    
    return new_value;
}

// Subscription management
SubscriptionHandle SharedState::subscribe(const std::string& pattern, SubscriptionCallback callback) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    Subscription sub;
    sub.handle = state->subscription_counter_++;
    sub.pattern = pattern;
    sub.callback = callback;
    
    state->subscriptions_[pattern].push_back(sub);
    
    ESP_LOGD(TAG, "Subscribed to %s (handle: %lu)", pattern.c_str(), sub.handle);
    
    return sub.handle;
}

void SharedState::unsubscribe(SubscriptionHandle handle) {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    for (auto& [pattern, sub_list] : state->subscriptions_) {
        auto it = std::remove_if(sub_list.begin(), sub_list.end(),
            [handle](const Subscription& sub) { return sub.handle == handle; });
        
        if (it != sub_list.end()) {
            sub_list.erase(it, sub_list.end());
            ESP_LOGD(TAG, "Unsubscribed handle: %lu", handle);
            return;
        }
    }
}

// Statistics
SharedState::Statistics SharedState::get_statistics() {
    auto* state = get_instance();
    SemaphoreGuard guard(state->mutex_);
    
    Statistics stats;
    stats.total_entries = state->storage_.size();
    stats.total_subscriptions = 0;
    
    for (const auto& [pattern, sub_list] : state->subscriptions_) {
        stats.total_subscriptions += sub_list.size();
    }
    
    // Calculate memory usage (approximate)
    stats.memory_used = sizeof(SharedState);
    for (const auto& [key, entry] : state->storage_) {
        stats.memory_used += key.length() + sizeof(StateEntry);
        if (std::holds_alternative<std::string>(entry.value)) {
            stats.memory_used += std::get<std::string>(entry.value).length();
        }
    }
    
    stats.memory_available = MAX_ENTRIES * (MAX_KEY_LENGTH + sizeof(StateEntry)) - stats.memory_used;
    
    return stats;
}

} // namespace ModuChill