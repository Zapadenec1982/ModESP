/**
 * @file rtc_module.cpp
 * @brief Implementation of RTCModule for ESP32 internal RTC
 */

#include "rtc_module.h"
#include "shared_state.h"
#include "event_bus.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>

static const char* TAG = "RTCModule";

// Static member initialization
time_t RTCModule::boot_timestamp_ = 0;

RTCModule::RTCModule() {
    // Record boot time
    if (boot_timestamp_ == 0) {
        boot_timestamp_ = time(nullptr);
    }
}

esp_err_t RTCModule::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "RTCModule already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing RTC Module");
    
    // Initialize timezone
    setenv("TZ", config_.timezone.c_str(), 1);
    tzset();
    
    // Check if time is already set (e.g., from previous boot)
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year < (2020 - 1900)) {
        // Time not set, use a default time for now
        // In production, this would sync with NTP or external RTC
        struct timeval tv = {
            .tv_sec = 1704067200,  // 2024-01-01 00:00:00
            .tv_usec = 0
        };
        settimeofday(&tv, nullptr);
        ESP_LOGW(TAG, "Time not set, using default: %s", get_time_string().c_str());
    } else {
        ESP_LOGI(TAG, "Current time: %s", get_time_string().c_str());
    }
    
    initialized_ = true;
    
    // Publish initial time state
    publish_time_state();
    
    // Publish initialization event
    nlohmann::json event_data = {
        {"module", "RTC"},
        {"status", "initialized"},
        {"time", get_timestamp()},
        {"uptime", get_uptime_seconds()}
    };
    EventBus::publish("system.rtc.initialized", event_data);
    
    return ESP_OK;
}

void RTCModule::update() {
    if (!initialized_) {
        return;
    }
    
    uint32_t now = get_uptime_seconds();
    
    // Publish time to SharedState periodically
    if (config_.publish_to_shared_state && 
        (now - last_publish_time_) >= config_.publish_interval_s) {
        publish_time_state();
        last_publish_time_ = now;
    }
}

void RTCModule::stop() {
    ESP_LOGI(TAG, "Stopping RTC Module");
    initialized_ = false;
}

void RTCModule::configure(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Configuring RTC Module");
    
    if (config.contains("timezone")) {
        config_.timezone = config["timezone"].get<std::string>();
        setenv("TZ", config_.timezone.c_str(), 1);
        tzset();
        ESP_LOGI(TAG, "Timezone set to: %s", config_.timezone.c_str());
    }
    
    if (config.contains("publish_to_shared_state")) {
        config_.publish_to_shared_state = config["publish_to_shared_state"].get<bool>();
    }
    
    if (config.contains("publish_interval_s")) {
        config_.publish_interval_s = config["publish_interval_s"].get<uint32_t>();
    }
}

bool RTCModule::is_healthy() const {
    return initialized_ && is_time_valid();
}

uint8_t RTCModule::get_health_score() const {
    if (!initialized_) return 0;
    if (!is_time_valid()) return 50;
    return 100;
}

void RTCModule::publish_time_state() {
    nlohmann::json time_data = {
        {"timestamp", get_timestamp()},
        {"time_string", get_time_string()},
        {"uptime_seconds", get_uptime_seconds()},
        {"timezone", config_.timezone},
        {"is_valid", is_time_valid()}
    };
    
    SharedState::set("state.time", time_data);
}

// Static methods

time_t RTCModule::get_timestamp() {
    return time(nullptr);
}

std::string RTCModule::get_time_string(const char* format) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), format, &timeinfo);
    return std::string(buffer);
}

esp_err_t RTCModule::set_time(time_t timestamp) {
    struct timeval tv = {
        .tv_sec = timestamp,
        .tv_usec = 0
    };
    
    if (settimeofday(&tv, nullptr) != 0) {
        ESP_LOGE(TAG, "Failed to set time");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Time set to: %s", get_time_string().c_str());
    
    // Publish event
    nlohmann::json event_data = {
        {"action", "time_set"},
        {"timestamp", timestamp},
        {"time_string", get_time_string()}
    };
    EventBus::publish("system.rtc.time_changed", event_data);
    
    return ESP_OK;
}

uint32_t RTCModule::get_uptime_seconds() {
    return esp_timer_get_time() / 1000000;  // Convert microseconds to seconds
}

bool RTCModule::is_time_valid() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    // Check if year is reasonable (after 2020)
    return timeinfo.tm_year >= (2020 - 1900);
}
