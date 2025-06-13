/**
 * @file rtc_module.h
 * @brief RTCModule - HAL module for Real-Time Clock management
 * 
 * Provides unified interface for time management using internal ESP32 RTC
 * or external RTC chips. Supports time synchronization, alarms, and
 * persistent timekeeping across power cycles.
 */

#pragma once

#include "base_module.h"
#include "esphal.h"
#include "shared_state.h"
#include <time.h>
#include <optional>

/**
 * @brief RTC source type
 */
enum class RTCSource {
    INTERNAL,      // ESP32 internal RTC
    DS3231,        // External DS3231 I2C RTC
    PCF8563,       // External PCF8563 I2C RTC
    DS1307,        // External DS1307 I2C RTC
    NTP            // Network Time Protocol
};

/**
 * @brief Time event for scheduling
 */
struct TimeEvent {
    std::string id;
    struct tm trigger_time;
    bool repeat;
    uint32_t repeat_interval_s;
    std::function<void()> callback;
};

/**
 * @brief RTCModule - Real-Time Clock management
 * 
 * Features:
 * - Multiple RTC source support
 * - Automatic source selection and fallback
 * - Time zone support
 * - Scheduled events
 * - Power loss detection
 * - SNTP synchronization
 */
class RTCModule : public BaseModule {
public:
    /**
     * @brief Constructor
     * @param hal Reference to HAL (for I2C access to external RTC)
     */
    explicit RTCModule(ESPhal& hal);
    
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
    uint32_t get_max_update_time_us() const override;
    
    // === Time management ===
    
    /**
     * @brief Get current time
     * @return Current time or nullopt if RTC not available
     */
    std::optional<struct tm> get_time() const;
    
    /**
     * @brief Set current time
     * @param time Time to set
     * @return ESP_OK on success
     */
    esp_err_t set_time(const struct tm& time);
    
    /**
     * @brief Get current timestamp
     * @return Unix timestamp or 0 if not available
     */
    time_t get_timestamp() const;
    
    /**
     * @brief Check if time is valid (synchronized)
     * @return true if time is valid
     */
    bool is_time_valid() const;
    
    /**
     * @brief Get time since last sync
     * @return Seconds since last successful sync
     */
    uint32_t get_time_since_sync() const;
    
    // === Scheduling ===
    
    /**
     * @brief Schedule a time-based event
     * @param event Event configuration
     * @return Event ID or empty string on failure
     */
    std::string schedule_event(const TimeEvent& event);
    
    /**
     * @brief Cancel scheduled event
     * @param event_id Event ID to cancel
     * @return ESP_OK on success
     */
    esp_err_t cancel_event(const std::string& event_id);
    
    // === Time zones ===
    
    /**
     * @brief Set time zone
     * @param tz Time zone string (e.g., "UTC-3")
     * @return ESP_OK on success
     */
    esp_err_t set_timezone(const std::string& tz);
    
    /**
     * @brief Get current time zone
     * @return Time zone string
     */
    std::string get_timezone() const;
    
    // === Diagnostics ===
    
    /**
     * @brief Get RTC source currently in use
     * @return Active RTC source
     */
    RTCSource get_active_source() const;
    
    /**
     * @brief Force sync with NTP
     * @return ESP_OK on success
     */
    esp_err_t sync_with_ntp();

private:
    // Reference to HAL for I2C access
    ESPhal& hal_;
    
    // Configuration
    struct Config {
        RTCSource primary_source = RTCSource::INTERNAL;
        RTCSource fallback_source = RTCSource::INTERNAL;
        std::string ntp_server = "pool.ntp.org";
        std::string timezone = "UTC";
        uint32_t sync_interval_s = 3600;  // 1 hour
        bool enable_scheduling = true;
    } config_;
    
    // State
    bool initialized_ = false;
    RTCSource active_source_ = RTCSource::INTERNAL;
    time_t last_sync_time_ = 0;
    bool time_valid_ = false;
    
    // Scheduled events
    std::vector<TimeEvent> scheduled_events_;
    uint32_t event_counter_ = 0;
    
    // Helper methods
    esp_err_t init_internal_rtc();
    esp_err_t init_external_rtc();
    esp_err_t init_ntp();
    void check_scheduled_events();
    void publish_time_state();
    bool is_time_match(const struct tm& current, const struct tm& target);
};
