/**
 * @file sensor_driver_registry.h
 * @brief Registry for dynamic sensor driver registration
 * 
 * This registry allows sensor drivers to self-register at compile time.
 * SensorModule uses this registry to create driver instances without
 * knowing about specific driver implementations.
 */

#pragma once

#include "sensor_driver_interface.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <esp_log.h>

/**
 * @brief Singleton registry for sensor drivers
 */
class SensorDriverRegistry {
public:
    /**
     * @brief Get registry instance
     */
    static SensorDriverRegistry& instance() {
        static SensorDriverRegistry registry;
        return registry;
    }
    
    /**
     * @brief Register a sensor driver factory
     * 
     * @param type Driver type identifier (e.g., "DS18B20")
     * @param factory Factory function to create driver instances
     * @return true if registered successfully
     */
    bool register_driver(const std::string& type, SensorDriverFactory factory) {
        if (factories_.find(type) != factories_.end()) {
            ESP_LOGW("DriverRegistry", "Driver type '%s' already registered", type.c_str());
            return false;
        }
        
        factories_[type] = factory;
        ESP_LOGI("DriverRegistry", "Registered sensor driver: %s", type.c_str());
        return true;
    }
    
    /**
     * @brief Create driver instance by type
     * 
     * @param type Driver type identifier
     * @return Driver instance or nullptr if type not found
     */
    std::unique_ptr<ISensorDriver> create_driver(const std::string& type) {
        auto it = factories_.find(type);
        if (it == factories_.end()) {
            ESP_LOGE("DriverRegistry", "Unknown sensor driver type: %s", type.c_str());
            return nullptr;
        }
        
        return it->second();
    }
    
    /**
     * @brief Get list of registered driver types
     */
    std::vector<std::string> get_registered_types() const {
        std::vector<std::string> types;
        for (const auto& [type, _] : factories_) {
            types.push_back(type);
        }
        return types;
    }
    
    /**
     * @brief Check if driver type is registered
     */
    bool has_driver(const std::string& type) const {
        return factories_.find(type) != factories_.end();
    }

private:
    SensorDriverRegistry() = default;
    std::unordered_map<std::string, SensorDriverFactory> factories_;
};

/**
 * @brief Helper class for automatic driver registration
 * 
 * Usage: Place this in driver source file:
 * static SensorDriverRegistrar<MyDriverClass> registrar("MY_DRIVER_TYPE");
 */
template<typename T>
class SensorDriverRegistrar {
public:
    explicit SensorDriverRegistrar(const std::string& type) {
        SensorDriverRegistry::instance().register_driver(type, []() {
            return std::make_unique<T>();
        });
    }
};