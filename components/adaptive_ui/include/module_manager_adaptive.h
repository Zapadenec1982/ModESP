// module_manager_adaptive.h
// Extension for Phase 5 Adaptive UI Architecture

#pragma once

#include "module_manager.h"
#include "base_driver.h"
#include <unordered_map>
#include <vector>

namespace ModuleManager {

/**
 * @brief Driver registry entry
 */
struct DriverEntry {
    std::unique_ptr<BaseDriver> driver;
    std::string manager_name;
    DriverState state = DriverState::CREATED;
    bool enabled = true;
};

/**
 * @brief Manager-Driver composition support
 */
class ManagerDriverSupport {
private:
    // Map manager name to its drivers
    std::unordered_map<std::string, std::vector<DriverEntry>> driver_registry;
    
public:
    /**
     * @brief Register a driver for a specific manager
     */
    esp_err_t registerDriver(const std::string& manager_name, 
                           std::unique_ptr<BaseDriver> driver);
    
    /**
     * @brief Get all drivers for a manager
     */
    std::vector<BaseDriver*> getDriversForManager(const std::string& manager_name);
    
    /**
     * @brief Initialize all drivers for a manager
     */
    esp_err_t initializeDrivers(const std::string& manager_name);
    
    /**
     * @brief Update driver states
     */
    void updateDrivers(const std::string& manager_name);
};

/**
 * @brief Extended registration for managers with drivers
 * 
 * Automatically discovers and registers drivers based on manifest
 * 
 * @param manager Manager module instance
 * @param driver_interface Interface name that drivers must implement
 * @return ESP_OK on success
 */
esp_err_t register_manager_with_drivers(std::unique_ptr<BaseModule> manager,
                                       const std::string& driver_interface);

/**
 * @brief Get manager-driver composition info
 */
struct CompositionInfo {
    std::string manager_name;
    std::string driver_interface;
    std::vector<std::string> registered_drivers;
    size_t total_ui_components;
    size_t loaded_ui_components;
};

std::vector<CompositionInfo> get_composition_info();

} // namespace ModuleManager
