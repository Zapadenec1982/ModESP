/**
 * @file actuator_driver_registry.h
 * @brief Registry for dynamic actuator driver registration
 * 
 * This registry allows actuator drivers to self-register at compile time.
 * ActuatorModule uses this registry to create driver instances without
 * knowing about specific driver implementations.
 */

#pragma once

#include "actuator_driver_interface.h"
#include <unordered_map>
#include <string>
#include <functional>
#include <esp_log.h>

/**
 * @brief Singleton registry for actuator drivers
 */
class ActuatorDriverRegistry {
public:
    /**
     * @brief Get registry instance
     */
    static ActuatorDriverRegistry& instance() {
        static ActuatorDriverRegistry registry;
        return registry;
    }
    
    /**
     * @brief Register an actuator driver factory
     * 
     * @param type Driver type identifier (e.g., "RELAY", "PWM")
     * @param factory Factory function to create driver instances
     * @return true if registered successfully
     */
    bool register_driver(const std::string& type, ActuatorDriverFactory factory) {
        if (factories_.find(type) != factories_.end()) {
            ESP_LOGW("ActuatorRegistry", "Driver type '%s' already registered", type.c_str());
            return false;
        }
        
        factories_[type] = factory;
        ESP_LOGI("ActuatorRegistry", "Registered actuator driver: %s", type.c_str());
        return true;
    }
    
    /**
     * @brief Create driver instance by type
     * 
     * @param type Driver type identifier
     * @return Driver instance or nullptr if type not found
     */
    std::unique_ptr<IActuatorDriver> create_driver(const std::string& type) {
        auto it = factories_.find(type);
        if (it == factories_.end()) {
            ESP_LOGE("ActuatorRegistry", "Unknown actuator driver type: %s", type.c_str());
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
    ActuatorDriverRegistry() = default;
    std::unordered_map<std::string, ActuatorDriverFactory> factories_;
};

/**
 * @brief Helper class for automatic driver registration
 * 
 * Usage: Place this in driver source file:
 * static ActuatorDriverRegistrar<MyDriverClass> registrar("MY_DRIVER_TYPE");
 */
template<typename T>
class ActuatorDriverRegistrar {
public:
    explicit ActuatorDriverRegistrar(const std::string& type) {
        ActuatorDriverRegistry::instance().register_driver(type, []() {
            return std::make_unique<T>();
        });
    }
};