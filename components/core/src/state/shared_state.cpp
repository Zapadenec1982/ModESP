#include "shared_state.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <algorithm>
#include <cstring>

static const char* TAG = "SharedState";

namespace SharedState {

// Storage entry structure
struct Entry {
    char key[MAX_KEY_LENGTH];          // Fixed-size key storage
    nlohmann::json value;              // Value (JSON can store dynamic data internally)
    uint64_t last_update = 0;          // Timestamp of last update
    uint32_t update_count = 0;         // Number of updates
    bool occupied = false;             // Slot is in use
};

// Subscription structure
struct Subscription {
    SubscriptionHandle handle;
    char pattern[MAX_KEY_LENGTH];      // Fixed-size pattern storage
    ChangeCallback callback;
    bool active = false;
};

// Static storage - all memory allocated at compile time
static std::array<Entry, CONFIG_SHARED_STATE_MAX_ENTRIES> storage;
static std::array<Subscription, 32> subscriptions;  // Max 32 subscriptions
static SemaphoreHandle_t mutex = nullptr;
static SubscriptionHandle next_handle = 1;

// Statistics
static size_t peak_used = 0;
static size_t total_sets = 0;
static size_t total_gets = 0;

// Helper: Find entry by key (returns nullptr if not found)
static Entry* find_entry(const char* key) {
    for (auto& entry : storage) {
        if (entry.occupied && strcmp(entry.key, key) == 0) {
            return &entry;
        }
    }
    return nullptr;
}

// Helper: Find free entry slot
static Entry* find_free_entry() {
    for (auto& entry : storage) {
        if (!entry.occupied) {
            return &entry;
        }
    }
    return nullptr;
}

// Helper: Count used entries
static size_t count_used_entries() {
    size_t count = 0;
    for (const auto& entry : storage) {
        if (entry.occupied) count++;
    }
    return count;
}

// Helper: Pattern matching
static bool matches_pattern(const char* pattern, const char* key) {
    if (pattern[0] == '\0' || strcmp(pattern, "*") == 0) {
        return true;
    }
    
    if (strcmp(pattern, key) == 0) {
        return true;
    }
    
    // Wildcard matching
    size_t pattern_len = strlen(pattern);
    if (pattern_len > 0 && pattern[pattern_len - 1] == '*') {
        return strncmp(pattern, key, pattern_len - 1) == 0;
    }
    
    return false;
}

// Helper: Notify subscribers (called outside mutex)
static void notify_subscribers(const char* key, const nlohmann::json& value) {
    for (const auto& sub : subscriptions) {
        if (sub.active && matches_pattern(sub.pattern, key)) {
            // Execute callback (ESP-IDF builds without exceptions)
            sub.callback(key, value);
        }
    }
}

esp_err_t init() {
    if (mutex != nullptr) {
        ESP_LOGW(TAG, "SharedState already initialized");
        return ESP_OK;
    }
    
    // Create mutex
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize storage (already zero-initialized by static, but be explicit)
    for (auto& entry : storage) {
        entry.occupied = false;
        entry.key[0] = '\0';
        entry.last_update = 0;
        entry.update_count = 0;
    }
    
    // Initialize subscriptions
    for (auto& sub : subscriptions) {
        sub.active = false;
        sub.pattern[0] = '\0';
    }
    
    ESP_LOGI(TAG, "SharedState initialized with %d entries (static allocation)", 
             CONFIG_SHARED_STATE_MAX_ENTRIES);
    
    return ESP_OK;
}

esp_err_t set(const std::string& key, const nlohmann::json& value) {
    if (mutex == nullptr) {
        ESP_LOGE(TAG, "SharedState not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key.length() >= MAX_KEY_LENGTH) {
        ESP_LOGE(TAG, "Key too long: %s", key.c_str());
        return ESP_ERR_INVALID_ARG;
    }
    
    // Take mutex
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Find existing or allocate new entry
    Entry* entry = find_entry(key.c_str());
    bool is_new = false;
    
    if (entry == nullptr) {
        // Allocate new entry
        entry = find_free_entry();
        if (entry == nullptr) {
            xSemaphoreGive(mutex);
            ESP_LOGE(TAG, "Storage full (%d entries)", CONFIG_SHARED_STATE_MAX_ENTRIES);
            return ESP_ERR_NO_MEM;
        }
        is_new = true;
    }
    
    // Store old value for comparison
    nlohmann::json old_value = entry->value;
    
    // Update entry
    if (is_new) {
        strncpy(entry->key, key.c_str(), MAX_KEY_LENGTH - 1);
        entry->key[MAX_KEY_LENGTH - 1] = '\0';
        entry->occupied = true;
    }
    
    entry->value = value;
    entry->last_update = esp_timer_get_time();
    entry->update_count++;
    total_sets++;
    
    // Update peak usage
    size_t used = count_used_entries();
    if (used > peak_used) {
        peak_used = used;
    }
    
    // Release mutex before callbacks
    xSemaphoreGive(mutex);
    
    // Notify subscribers if value changed
    if (is_new || old_value != value) {
        notify_subscribers(key.c_str(), value);
    }
    
    ESP_LOGD(TAG, "Set %s = %s", key.c_str(), value.dump().c_str());
    
    return ESP_OK;
}

esp_err_t get(const std::string& key, nlohmann::json& value) {
    if (mutex == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    Entry* entry = find_entry(key.c_str());
    if (entry == nullptr) {
        xSemaphoreGive(mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    value = entry->value;
    total_gets++;
    
    xSemaphoreGive(mutex);
    
    return ESP_OK;
}

bool exists(const std::string& key) {
    if (mutex == nullptr) return false;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    bool found = find_entry(key.c_str()) != nullptr;
    
    xSemaphoreGive(mutex);
    
    return found;
}

esp_err_t remove(const std::string& key) {
    if (mutex == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    Entry* entry = find_entry(key.c_str());
    if (entry == nullptr) {
        xSemaphoreGive(mutex);
        return ESP_ERR_NOT_FOUND;
    }
    
    // Clear entry
    entry->occupied = false;
    entry->key[0] = '\0';
    entry->value = nlohmann::json{};
    entry->last_update = 0;
    entry->update_count = 0;
    
    xSemaphoreGive(mutex);
    
    ESP_LOGD(TAG, "Removed key: %s", key.c_str());
    
    return ESP_OK;
}
bool has_changed(const std::string& pattern, uint64_t since_timestamp) {
    if (mutex == nullptr) return false;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    
    bool changed = false;
    for (const auto& entry : storage) {
        if (entry.occupied && 
            matches_pattern(pattern.c_str(), entry.key) && 
            entry.last_update > since_timestamp) {
            changed = true;
            break;
        }
    }
    
    xSemaphoreGive(mutex);
    
    return changed;
}

uint64_t get_last_update_time(const std::string& key) {
    if (mutex == nullptr) return 0;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    
    uint64_t timestamp = 0;
    Entry* entry = find_entry(key.c_str());
    if (entry != nullptr) {
        timestamp = entry->last_update;
    }
    
    xSemaphoreGive(mutex);
    
    return timestamp;
}

esp_err_t compare_and_set(const std::string& key, const nlohmann::json& expected, 
                         const nlohmann::json& new_value) {
    if (mutex == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    Entry* entry = find_entry(key.c_str());
    if (entry == nullptr || entry->value != expected) {
        xSemaphoreGive(mutex);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Update value
    entry->value = new_value;
    entry->last_update = esp_timer_get_time();
    entry->update_count++;
    total_sets++;
    
    xSemaphoreGive(mutex);
    
    // Notify subscribers
    notify_subscribers(key.c_str(), new_value);
    
    return ESP_OK;
}

esp_err_t increment(const std::string& key, double delta) {
    if (mutex == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (key.length() >= MAX_KEY_LENGTH) {
        ESP_LOGE(TAG, "Key too long: %s", key.c_str());
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    Entry* entry = find_entry(key.c_str());
    nlohmann::json new_value;
    bool is_new = false;
    
    if (entry == nullptr) {
        // Create new entry with delta
        entry = find_free_entry();
        if (entry == nullptr) {
            xSemaphoreGive(mutex);
            ESP_LOGE(TAG, "Storage full (%d entries)", CONFIG_SHARED_STATE_MAX_ENTRIES);
            return ESP_ERR_NO_MEM;
        }
        
        strncpy(entry->key, key.c_str(), MAX_KEY_LENGTH - 1);
        entry->key[MAX_KEY_LENGTH - 1] = '\0';
        entry->occupied = true;
        new_value = delta;
        is_new = true;
    } else {
        // Increment existing value
        if (entry->value.is_number()) {
            double current = entry->value.get<double>();
            new_value = current + delta;
        } else {
            xSemaphoreGive(mutex);
            ESP_LOGE(TAG, "Cannot increment non-numeric value: %s", key.c_str());
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    // Update entry
    entry->value = new_value;
    entry->last_update = esp_timer_get_time();
    entry->update_count++;
    total_sets++;
    
    // Update peak usage if new entry
    if (is_new) {
        size_t used = count_used_entries();
        if (used > peak_used) {
            peak_used = used;
        }
    }
    
    xSemaphoreGive(mutex);
    
    // Notify subscribers
    notify_subscribers(key.c_str(), new_value);
    
    ESP_LOGD(TAG, "Incremented %s by %.2f = %.2f", 
             key.c_str(), delta, new_value.get<double>());
    
    return ESP_OK;
}

SubscriptionHandle subscribe(const std::string& pattern, ChangeCallback callback) {
    if (xTaskGetCurrentTaskHandle() != xTaskGetHandle("main")) {
        ESP_LOGE(TAG, "Subscribe must be called from main task");
        return 0;
    }
    
    if (pattern.length() >= MAX_KEY_LENGTH) {
        ESP_LOGE(TAG, "Pattern too long: %s", pattern.c_str());
        return 0;
    }
    
    // Find free subscription slot
    Subscription* sub = nullptr;
    for (auto& s : subscriptions) {
        if (!s.active) {
            sub = &s;
            break;
        }
    }
    
    if (sub == nullptr) {
        ESP_LOGE(TAG, "No free subscription slots");
        return 0;
    }
    
    // Configure subscription
    sub->handle = next_handle++;
    strncpy(sub->pattern, pattern.c_str(), MAX_KEY_LENGTH - 1);
    sub->pattern[MAX_KEY_LENGTH - 1] = '\0';
    sub->callback = callback;
    sub->active = true;
    
    ESP_LOGD(TAG, "Subscribed to %s (handle: %lu)", pattern.c_str(), sub->handle);
    
    return sub->handle;
}

esp_err_t unsubscribe(SubscriptionHandle handle) {
    for (auto& sub : subscriptions) {
        if (sub.active && sub.handle == handle) {
            sub.active = false;
            sub.pattern[0] = '\0';
            return ESP_OK;
        }
    }
    
    return ESP_ERR_NOT_FOUND;
}

std::vector<std::string> get_keys(const std::string& pattern) {
    std::vector<std::string> keys;
    
    if (mutex == nullptr) return keys;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return keys;
    }
    
    for (const auto& entry : storage) {
        if (entry.occupied && 
            (pattern.empty() || matches_pattern(pattern.c_str(), entry.key))) {
            keys.push_back(entry.key);
        }
    }
    
    xSemaphoreGive(mutex);
    
    return keys;
}

void clear() {
    if (mutex == nullptr) return;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    
    // Clear all entries
    for (auto& entry : storage) {
        if (entry.occupied) {
            entry.occupied = false;
            entry.key[0] = '\0';
            entry.value = nlohmann::json{};
            entry.last_update = 0;
            entry.update_count = 0;
        }
    }
    
    xSemaphoreGive(mutex);
    
    ESP_LOGI(TAG, "SharedState cleared");
}

size_t get_entry_count() {
    if (mutex == nullptr) return 0;
    
    if (xSemaphoreTake(mutex, portMAX_DELAY) != pdTRUE) {
        return 0;
    }
    
    size_t count = count_used_entries();
    
    xSemaphoreGive(mutex);
    
    return count;
}

size_t get_subscription_count() {
    size_t count = 0;
    for (const auto& sub : subscriptions) {
        if (sub.active) count++;
    }
    return count;
}

size_t get_total_sets() {
    return total_sets;
}

size_t get_total_gets() {
    return total_gets;
}

Stats get_stats() {
    Stats stats;
    
    stats.capacity = CONFIG_SHARED_STATE_MAX_ENTRIES;
    stats.used = get_entry_count();
    stats.peak_used = peak_used;
    stats.total_sets = total_sets;
    stats.total_gets = total_gets;
    stats.subscriptions = get_subscription_count();
    
    return stats;
}

} // namespace SharedState