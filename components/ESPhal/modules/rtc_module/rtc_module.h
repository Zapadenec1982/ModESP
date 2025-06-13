/**
 * @file rtc_module.h
 * @brief RTCModule - HAL module for Real-Time Clock management
 * 
 * Simple RTC module for first system launch. Uses ESP32 internal RTC
 * to provide timestamps for logging and time tracking functionality.
 */

#pragma once

#include "base_module.h"
#include "shared_state.h"
#include <time.h>
#include <sys/time.h>

/**
 * @brief RTCModule - Real-Time Clock management
 * 
 * Basic features for MVP:
 * - ESP32 internal RTC usage
 * - Timestamp generation for logs
 * - Uptime tracking
 * - Simple time get/set operations
 * - SharedState time publishing
 */
class RTCModule : public BaseModule {
public:
    RTCModule();
    ~RTCModule() override = default;
    
    // Non-copyable, non-movable
    RTCModule(const RTCModule&) = delete;
    RTCModule& operator=(const RTCModule&) = delete;
    RTCModule(RTCModule&&) = delete;
    RTCModule& operator=(RTCModule&&) = delete;

    // === BaseModule interface ===
    const char* get_name() const override { return "RTCModule"; }
    esp_err_t init() override;
    void update() override;
    void stop() override;
    void configure(const nlohmann::json& config) override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    uint32_t get_max_update_time_us() const override { return 1000; } // 1ms max
    
    // === Basic time operations ===
    
    /**
     * @brief Get current timestamp
     * @return Unix timestamp
     */
    static time_t get_timestamp();
    
    /**
     * @brief Get formatted time string
     * @param format Format string (default: "%Y-%m-%d %H:%M:%S")
     * @return Formatted time string
     */
    static std::string get_time_string(const char* format = "%Y-%m-%d %H:%M:%S");
    
    /**
     * @brief Set system time
     * @param timestamp Unix timestamp to set
     * @return ESP_OK on success
     */
    static esp_err_t set_time(time_t timestamp);
    
    /**
     * @brief Get system uptime
     * @return Uptime in seconds
     */
    static uint32_t get_uptime_seconds();
    
    /**
     * @brief Check if time is valid (not default 1970)
     * @return true if time has been set
     */
    static bool is_time_valid();

private:
    // Configuration
    struct Config {
        std::string timezone = "UTC";
        bool publish_to_shared_state = true;
        uint32_t publish_interval_s = 60;  // Publish time every minute
    } config_;
    
    // State
    bool initialized_ = false;
    uint32_t last_publish_time_ = 0;
    static time_t boot_timestamp_;  // Time when system booted
    
    // Helper methods
    void publish_time_state();
};
