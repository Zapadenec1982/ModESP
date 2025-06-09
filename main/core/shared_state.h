#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <variant>
#include <functional>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_timer.h>

namespace ModuChill {

// Supported value types
using StateValue = std::variant<int, float, bool, std::string>;

// Subscription callback (receives key that changed)
using SubscriptionCallback = std::function<void(const std::string& key)>;

// Handle for unsubscribing
using SubscriptionHandle = uint32_t;

/**
 * RAII Semaphore guard
 */
class SemaphoreGuard {
private:
    SemaphoreHandle_t mutex_;
public:
    explicit SemaphoreGuard(SemaphoreHandle_t mutex) : mutex_(mutex) {
        xSemaphoreTake(mutex_, portMAX_DELAY);
    }
    ~SemaphoreGuard() {
        xSemaphoreGive(mutex_);
    }
};

/**
 * SharedState - Thread-safe key-value storage
 * 
 * Centralized state storage with type safety and change notifications.
 * All modules can read/write state without knowing about each other.
 */
class SharedState {
private:
    struct StateEntry {
        StateValue value;
        uint32_t last_update;
        uint32_t update_count = 0;
    };
    
    struct Subscription {
        SubscriptionHandle handle;
        std::string pattern;
        SubscriptionCallback callback;
    };
    
    // Storage
    std::unordered_map<std::string, StateEntry> storage_;
    
    // Subscriptions by pattern
    std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
    SubscriptionHandle subscription_counter_;
    
    // Thread safety
    SemaphoreHandle_t mutex_;
    
    // Configuration
    static constexpr size_t MAX_KEY_LENGTH = 32;
    static constexpr size_t MAX_STRING_VALUE = 128;
    static constexpr size_t MAX_ENTRIES = 64;
    
    // Singleton
    static SharedState* instance_;
    
    SharedState();
    ~SharedState();
    
    // Internal methods
    bool set_internal(const std::string& key, const StateValue& value);
    std::optional<StateValue> get_internal(const std::string& key) const;
    void trigger_subscriptions(const std::string& key);
    
public:
    // Initialization
    static void init();
    static void deinit();
    
    // Basic operations
    template<typename T>
    static void set(const std::string& key, const T& value) {
        auto* state = get_instance();
        bool changed = false;
        
        {
            SemaphoreGuard guard(state->mutex_);
            changed = state->set_internal(key, StateValue(value));
        }
        
        if (changed) {
            state->trigger_subscriptions(key);
        }
    }
    
    template<typename T>
    static std::optional<T> get(const std::string& key) {
        auto* state = get_instance();
        SemaphoreGuard guard(state->mutex_);
        
        auto value = state->get_internal(key);
        if (value.has_value()) {
            try {
                return std::get<T>(*value);
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
    static bool exists(const std::string& key);
    static bool remove(const std::string& key);
    
    // Bulk operations
    static size_t size();
    static void clear();
    static std::vector<std::string> get_keys(const std::string& pattern = "");
    
    // Change tracking
    static bool has_changed(const std::string& pattern, uint32_t since_timestamp);
    static uint32_t get_last_change_time(const std::string& key);
    
    // Atomic operations
    static bool compare_and_swap(const std::string& key, float expected, float new_value);
    static float increment(const std::string& key, float delta = 1.0f);
    static float decrement(const std::string& key, float delta = 1.0f) {
        return increment(key, -delta);
    }
    
    // Subscriptions
    static SubscriptionHandle subscribe(const std::string& pattern, SubscriptionCallback callback);
    static void unsubscribe(SubscriptionHandle handle);
    
    // Statistics
    struct Statistics {
        size_t total_entries;
        size_t total_subscriptions;
        size_t memory_used;
        size_t memory_available;
    };
    static Statistics get_statistics();
    
private:
    static SharedState* get_instance();
};

} // namespace ModuChill