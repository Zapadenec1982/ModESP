/**
 * @file sensor_module.cpp
 * @brief Implementation of SensorModule with modular driver architecture
 * 
 * SensorModule uses a driver registry system where each sensor type is
 * a self-contained driver component with its own logic, configuration,
 * and UI schema.
 */

#include "sensor_module.h"
#include "sensor_driver_registry.h"
#include "shared_state.h"
#include "event_bus.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>

static const char* TAG = "SensorModule";

// === SensorModule implementation ===

SensorModule::SensorModule(ESPhal& hal) 
    : hal_(hal) {
    ESP_LOGI(TAG, "SensorModule created");
}

esp_err_t SensorModule::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "SensorModule already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing SensorModule...");    
    // Log available drivers
    auto& registry = SensorDriverRegistry::instance();
    auto available_drivers = registry.get_registered_types();
    
    ESP_LOGI(TAG, "Available sensor drivers: %zu", available_drivers.size());
    for (const auto& driver_type : available_drivers) {
        ESP_LOGI(TAG, "  - %s", driver_type.c_str());
    }
    
    // Configuration will be loaded later via configure()
    initialized_ = true;
    
    ESP_LOGI(TAG, "SensorModule initialized successfully");
    return ESP_OK;
}

void SensorModule::configure(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Configuring SensorModule");
    
    // Clear existing sensors
    sensors_.clear();
    
    // Parse global settings
    if (config.contains("poll_interval_ms")) {
        poll_interval_ms_ = config["poll_interval_ms"].get<uint32_t>();
        ESP_LOGI(TAG, "Poll interval: %u ms", poll_interval_ms_);
    }
    
    if (config.contains("publish_on_error")) {
        publish_on_error_ = config["publish_on_error"].get<bool>();
    }    
    // Create sensors from configuration
    if (config.contains("sensors")) {
        const auto& sensors_array = config["sensors"];
        
        for (const auto& sensor_config : sensors_array) {
            esp_err_t ret = create_sensor_from_config(sensor_config);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create sensor");
            }
        }
    }
    
    ESP_LOGI(TAG, "Configured %zu sensors", sensors_.size());
}

esp_err_t SensorModule::create_sensor_from_config(const nlohmann::json& sensor_config) {
    try {
        // Parse sensor configuration
        SensorConfig config;
        config.role = sensor_config["role"].get<std::string>();
        config.type = sensor_config["type"].get<std::string>();
        config.publish_key = sensor_config["publish_key"].get<std::string>();
        config.config = sensor_config.value("config", nlohmann::json::object());
        
        ESP_LOGI(TAG, "Creating sensor: role='%s', type='%s'", 
                 config.role.c_str(), config.type.c_str());
        
        // Create driver through registry
        auto& registry = SensorDriverRegistry::instance();
        auto driver = registry.create_driver(config.type);        
        if (!driver) {
            ESP_LOGE(TAG, "Unknown sensor type: %s", config.type.c_str());
            return ESP_ERR_NOT_FOUND;
        }
        
        // Initialize driver with HAL and config
        esp_err_t ret = driver->init(hal_, config.config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize driver: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // Create sensor instance
        SensorInstance instance;
        instance.driver = std::move(driver);
        instance.config = std::move(config);
        instance.last_reading = {0.0f, "°C", 0, false, "Not read yet"};
        instance.poll_failures = 0;
        
        sensors_.push_back(std::move(instance));
        
        ESP_LOGI(TAG, "Sensor created successfully: %s", instance.config.role.c_str());
        return ESP_OK;
        
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "JSON parsing error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Error creating sensor: %s", e.what());
        return ESP_FAIL;
    }
}
void SensorModule::update() {
    if (!initialized_) {
        return;
    }
    
    update_count_++;
    
    // Poll all sensors
    for (auto& sensor : sensors_) {
        try {
            // Read sensor value
            ::SensorReading reading = sensor.driver->read();
            
            // Update last reading
            sensor.last_reading = reading;
            
            // Publish to SharedState
            if (reading.is_valid || publish_on_error_) {
                publish_sensor_data(sensor, reading);
            }
            
            // Reset failure counter on successful read
            if (reading.is_valid) {
                sensor.poll_failures = 0;
            } else {
                sensor.poll_failures++;
                total_errors_++;
            }
            
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Exception reading sensor %s: %s", 
                     sensor.config.role.c_str(), e.what());
            sensor.poll_failures++;
            total_errors_++;
        }
    }
}
void SensorModule::publish_sensor_data(const SensorInstance& sensor, 
                                      const ::SensorReading& reading) {
    // Publish to SharedState
    SharedState::set(sensor.config.publish_key, reading.to_json());
    
    // Publish event if significant change
    nlohmann::json event_data = {
        {"role", sensor.config.role},
        {"type", sensor.config.type},
        {"reading", reading.to_json()}
    };
    
    EventBus::publish("sensor.reading", event_data);
}

void SensorModule::stop() {
    ESP_LOGI(TAG, "Stopping SensorModule");
    
    // Clear all sensors
    sensors_.clear();
    
    initialized_ = false;
}

bool SensorModule::is_healthy() const {
    if (!initialized_) {
        return false;
    }
    
    // Check if any sensor has too many failures
    for (const auto& sensor : sensors_) {
        if (sensor.poll_failures > 10) {
            return false;
        }
    }
    
    return true;
}

uint8_t SensorModule::get_health_score() const {
    if (!initialized_ || sensors_.empty()) {
        return 0;
    }
    
    // Calculate health based on successful sensors
    size_t healthy_count = 0;
    for (const auto& sensor : sensors_) {
        if (sensor.driver->is_available() && sensor.poll_failures < 3) {
            healthy_count++;
        }
    }
    
    return (healthy_count * 100) / sensors_.size();
}
uint32_t SensorModule::get_max_update_time_us() const {
    // Estimate based on sensor count and type
    // DS18B20 can take up to 750ms for conversion
    return sensors_.size() * 800000; // 800ms per sensor worst case
}

std::optional<::SensorReading> SensorModule::get_sensor_reading(const std::string& role) const {
    auto it = std::find_if(sensors_.begin(), sensors_.end(),
        [&role](const SensorInstance& sensor) {
            return sensor.config.role == role;
        });
    
    if (it != sensors_.end()) {
        return it->last_reading;
    }
    
    return std::nullopt;
}

std::optional<SensorConfig> SensorModule::get_sensor_config(const std::string& role) const {
    auto it = std::find_if(sensors_.begin(), sensors_.end(),
        [&role](const SensorInstance& sensor) {
            return sensor.config.role == role;
        });
    
    if (it != sensors_.end()) {
        return it->config;
    }
    
    return std::nullopt;
}

esp_err_t SensorModule::poll_sensors_now() {
    ESP_LOGI(TAG, "Forcing immediate sensor poll");
    update();
    return ESP_OK;
}

std::vector<std::string> SensorModule::get_available_drivers() const {
    return SensorDriverRegistry::instance().get_registered_types();
}