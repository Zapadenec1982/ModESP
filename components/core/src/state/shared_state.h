#pragma once

#include <array>
#include <string_view>
#include <functional>
#include <variant>
#include <optional>
#include <cstring>
#include "esp_err.h"
#include "nlohmann/json.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace SharedState {

/**
 * @brief Maximum number of key-value pairs
 * Compile-time configurable for different hardware configurations
 */
#ifndef CONFIG_SHARED_STATE_MAX_ENTRIES
#define CONFIG_SHARED_STATE_MAX_ENTRIES 64
#endif

/**
 * @brief Maximum key length (including null terminator)
 */
static constexpr size_t MAX_KEY_LENGTH = 32;

/**
 * @brief Change notification callback
 * @param key The key that changed
 * @param value The new value
 */
using ChangeCallback = std::function<void(const std::string& key, const nlohmann::json& value)>;

/**
 * @brief Subscription handle for unsubscribing
 */
using SubscriptionHandle = uint32_t;

/**
 * @brief Initialize SharedState
 * 
 * Must be called once during system initialization.
 * Allocates all memory statically.
 * 
 * @return ESP_OK on success
 */
esp_err_t init();

/**
 * @brief Set key-value pair
 * 
 * Thread-safe. Updates value and notifies subscribers if changed.
 * 
 * @param key Key (max 31 characters)
 * @param value JSON value to store
 * @return ESP_OK on success, ESP_ERR_NO_MEM if storage full
 */
esp_err_t set(const std::string& key, const nlohmann::json& value);

/**
 * @brief Get value by key
 * 
 * Thread-safe.
 * 
 * @param key Key to lookup
 * @param value Output parameter for value
 * @return ESP_OK if found, ESP_ERR_NOT_FOUND if not exists
 */
esp_err_t get(const std::string& key, nlohmann::json& value);

/**
 * @brief Check if key exists
 * 
 * Thread-safe.
 * 
 * @param key Key to check
 * @return true if exists
 */
bool exists(const std::string& key);

/**
 * @brief Remove key-value pair
 * 
 * Thread-safe.
 * 
 * @param key Key to remove
 * @return ESP_OK if removed, ESP_ERR_NOT_FOUND if not exists
 */
esp_err_t remove(const std::string& key);

/**
 * @brief Check if any key matching pattern has changed
 * 
 * Pattern matching:
 * - "*" matches all keys
 * - "sensor.*" matches keys starting with "sensor."
 * - Exact key matches only that key
 * 
 * @param pattern Key pattern
 * @param since_timestamp Check changes since this timestamp
 * @return true if any matching key changed
 */
bool has_changed(const std::string& pattern, uint64_t since_timestamp);

/**
 * @brief Get last update timestamp for key
 * 
 * @param key Key to check
 * @return Timestamp of last update, 0 if not exists
 */
uint64_t get_last_update_time(const std::string& key);

/**
 * @brief Compare and set atomically
 * 
 * Only sets new value if current value equals expected.
 * 
 * @param key Key to update
 * @param expected Expected current value
 * @param new_value New value to set
 * @return ESP_OK if updated, ESP_ERR_INVALID_STATE if current != expected
 */
esp_err_t compare_and_set(const std::string& key, const nlohmann::json& expected, 
                         const nlohmann::json& new_value);

/**
 * @brief Atomic increment operation for numeric values
 * 
 * Increments numeric value by delta. Creates key with delta if not exists.
 * Works with int, float, and double values stored as JSON.
 * 
 * @param key Key to increment
 * @param delta Value to add (default: 1.0)
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if value is not numeric
 */
esp_err_t increment(const std::string& key, double delta = 1.0);

/**
 * @brief Subscribe to key changes
 * 
 * Must be called from main task only.
 * Callbacks are called synchronously when values change.
 * 
 * @param pattern Key pattern to match
 * @param callback Function to call on changes
 * @return Subscription handle
 */
SubscriptionHandle subscribe(const std::string& pattern, ChangeCallback callback);

/**
 * @brief Unsubscribe from changes
 * 
 * @param handle Subscription handle from subscribe()
 * @return ESP_OK if unsubscribed
 */
esp_err_t unsubscribe(SubscriptionHandle handle);

/**
 * @brief Get all keys matching pattern
 * 
 * @param pattern Key pattern (empty = all keys)
 * @return Vector of matching keys
 */
std::vector<std::string> get_keys(const std::string& pattern = "");

/**
 * @brief Clear all entries
 * 
 * Removes all key-value pairs and notifies subscribers.
 */
void clear();

/**
 * @brief Get number of stored entries
 * @return Current number of key-value pairs
 */
size_t get_entry_count();

/**
 * @brief Get number of active subscriptions
 * @return Current number of subscriptions
 */
size_t get_subscription_count();

/**
 * @brief Get total number of set operations
 * @return Total sets since init
 */
size_t get_total_sets();

/**
 * @brief Get total number of get operations
 * @return Total gets since init
 */
size_t get_total_gets();

/**
 * @brief Storage statistics
 */
struct Stats {
    size_t capacity;        // Maximum entries
    size_t used;           // Current entries
    size_t peak_used;      // Maximum entries ever used
    size_t total_sets;     // Total set operations
    size_t total_gets;     // Total get operations
    size_t subscriptions;  // Active subscriptions
};

/**
 * @brief Get storage statistics
 * @return Current statistics
 */
Stats get_stats();

/**
 * @brief Typed helper method for setting simple values
 * 
 * Convenience method for setting primitive types without creating JSON manually.
 * 
 * @param key Key to set
 * @param value Value of any primitive type (int, float, bool, string)
 * @return ESP_OK on success
 */
template<typename T>
esp_err_t set_typed(const std::string& key, T value) {
    return set(key, nlohmann::json(value));
}

/**
 * @brief Typed helper method for getting simple values
 * 
 * Convenience method for getting primitive types with type safety.
 * 
 * @param key Key to get
 * @return std::optional<T> containing value if found and type matches
 */
template<typename T>
std::optional<T> get_typed(const std::string& key) {
    nlohmann::json value;
    if (get(key, value) == ESP_OK) {
        try {
            return value.get<T>();
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

} // namespace SharedState