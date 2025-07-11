/**
 * @file event_helpers.h
 * @brief Type-safe event publishing using generated constants
 * 
 * Provides helper functions and macros for working with events
 * in a type-safe manner using the generated event constants.
 */

#ifndef EVENT_HELPERS_H
#define EVENT_HELPERS_H

#include "event_bus.h"
#include "generated_system_contract.h"
#include "nlohmann/json.hpp"

namespace ModESP {

/**
 * @brief Type-safe event publisher
 * 
 * Wraps EventBus::publish with compile-time event name checking
 */
class EventPublisher {
public:
    /**
     * @brief Publish sensor calibration complete event
     */
    static esp_err_t publishSensorCalibrationComplete(const nlohmann::json& data = {}) {
        return EventBus::publish(Events::SENSOR_CALIBRATION_COMPLETE, data);
    }
    
    /**
     * @brief Publish sensor error event
     * @param sensorRole Sensor role/identifier
     * @param errorMessage Error description
     * @param errorCode Optional error code
     */
    static esp_err_t publishSensorError(const std::string& sensorRole, 
                                       const std::string& errorMessage,
                                       int errorCode = 0) {
        nlohmann::json data = {
            {"sensor_role", sensorRole},
            {"error_message", errorMessage},
            {"error_code", errorCode},
            {"timestamp", esp_timer_get_time()}
        };
        return EventBus::publish(Events::SENSOR_ERROR, data, EventBus::Priority::HIGH);
    }
    
    /**
     * @brief Publish sensor reading updated event
     * @param sensorRole Sensor role/identifier
     * @param value Sensor value
     * @param unit Unit of measurement
     */
    static esp_err_t publishSensorReadingUpdated(const std::string& sensorRole,
                                                 float value,
                                                 const std::string& unit) {
        nlohmann::json data = {
            {"sensor_role", sensorRole},
            {"value", value},
            {"unit", unit},
            {"timestamp", esp_timer_get_time()}
        };
        return EventBus::publish(Events::SENSOR_READING_UPDATED, data);
    }
    
    /**
     * @brief Publish system health warning
     * @param module Module name
     * @param healthScore Current health score (0-100)
     * @param reason Warning reason
     */
    static esp_err_t publishSystemHealthWarning(const std::string& module,
                                                uint8_t healthScore,
                                                const std::string& reason) {
        nlohmann::json data = {
            {"module", module},
            {"health_score", healthScore},
            {"reason", reason},
            {"timestamp", esp_timer_get_time()}
        };
        return EventBus::publish(Events::SYSTEM_HEALTH_WARNING, data, 
                                EventBus::Priority::HIGH);
    }
    
    /**
     * @brief Publish system heartbeat
     * @param moduleCount Number of active modules
     * @param systemHealth Overall system health (0-100)
     * @param uptime System uptime in seconds
     */
    static esp_err_t publishSystemHeartbeat(size_t moduleCount,
                                           uint8_t systemHealth,
                                           uint32_t uptime) {
        nlohmann::json data = {
            {"module_count", moduleCount},
            {"system_health", systemHealth},
            {"uptime_seconds", uptime},
            {"timestamp", esp_timer_get_time()}
        };
        return EventBus::publish(Events::SYSTEM_HEARTBEAT, data);
    }
};

/**
 * @brief Type-safe event subscriber
 * 
 * Provides convenient methods for subscribing to specific events
 */
class EventSubscriber {
public:
    /**
     * @brief Subscribe to sensor error events
     */
    static EventBus::SubscriptionHandle onSensorError(
        std::function<void(const std::string& role, const std::string& error, int code)> handler) {
        
        return EventBus::subscribe(Events::SENSOR_ERROR, 
            [handler](const EventBus::Event& event) {
                auto role = event.data.value("sensor_role", "unknown");
                auto error = event.data.value("error_message", "unknown error");
                auto code = event.data.value("error_code", 0);
                handler(role, error, code);
            });
    }
    
    /**
     * @brief Subscribe to sensor reading updates
     */
    static EventBus::SubscriptionHandle onSensorReading(
        std::function<void(const std::string& role, float value, const std::string& unit)> handler) {
        
        return EventBus::subscribe(Events::SENSOR_READING_UPDATED,
            [handler](const EventBus::Event& event) {
                auto role = event.data.value("sensor_role", "unknown");
                auto value = event.data.value("value", 0.0f);
                auto unit = event.data.value("unit", "");
                handler(role, value, unit);
            });
    }
    
    /**
     * @brief Subscribe to system health warnings
     */
    static EventBus::SubscriptionHandle onHealthWarning(
        std::function<void(const std::string& module, uint8_t score, const std::string& reason)> handler) {
        
        return EventBus::subscribe(Events::SYSTEM_HEALTH_WARNING,
            [handler](const EventBus::Event& event) {
                auto module = event.data.value("module", "unknown");
                auto score = event.data.value("health_score", uint8_t(0));
                auto reason = event.data.value("reason", "unknown");
                handler(module, score, reason);
            });
    }
    
    /**
     * @brief Subscribe to system heartbeat
     */
    static EventBus::SubscriptionHandle onSystemHeartbeat(
        std::function<void(size_t modules, uint8_t health, uint32_t uptime)> handler) {
        
        return EventBus::subscribe(Events::SYSTEM_HEARTBEAT,
            [handler](const EventBus::Event& event) {
                auto modules = event.data.value("module_count", size_t(0));
                auto health = event.data.value("system_health", uint8_t(0));
                auto uptime = event.data.value("uptime_seconds", uint32_t(0));
                handler(modules, health, uptime);
            });
    }
};

/**
 * @brief Macro for compile-time event name checking
 * 
 * Usage: PUBLISH_EVENT(SENSOR_ERROR, data);
 * Expands to: EventBus::publish(Events::SENSOR_ERROR, data);
 */
#define PUBLISH_EVENT(event_name, ...) \
    EventBus::publish(ModESP::Events::event_name, ##__VA_ARGS__)

/**
 * @brief Macro for subscribing with compile-time checking
 * 
 * Usage: SUBSCRIBE_EVENT(SENSOR_ERROR, handler);
 */
#define SUBSCRIBE_EVENT(event_name, handler) \
    EventBus::subscribe(ModESP::Events::event_name, handler)

} // namespace ModESP

#endif // EVENT_HELPERS_H
