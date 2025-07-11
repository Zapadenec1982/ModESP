/**
 * @file module_lifecycle.cpp
 * @brief Module lifecycle management - creation and registration
 * 
 * Combines module factory and registry functionality for
 * creating and registering modules in the system.
 */

#include "module_lifecycle.h"
#include "module_manager.h"
#include "logger_module.h"
#include <esp_log.h>
#include <memory>

static const char* TAG = "ModuleLifecycle";

namespace ModESP {

// ============================================================================
// Module Factory implementation
// ============================================================================

ModuleFactory& ModuleFactory::getInstance() {
    static ModuleFactory instance;
    return instance;
}

bool ModuleFactory::registerModule(const std::string& moduleName, ModuleCreator creator) {
    if (m_creators.find(moduleName) != m_creators.end()) {
        ESP_LOGW(TAG, "Module %s already registered", moduleName.c_str());
        return false;
    }
    
    m_creators[moduleName] = creator;
    ESP_LOGI(TAG, "Registered module creator: %s", moduleName.c_str());
    return true;
}

std::unique_ptr<BaseModule> ModuleFactory::createModule(const std::string& moduleName) {
    auto it = m_creators.find(moduleName);
    if (it == m_creators.end()) {
        ESP_LOGE(TAG, "No creator registered for module: %s", moduleName.c_str());
        return nullptr;
    }
    
    ESP_LOGI(TAG, "Creating module instance: %s", moduleName.c_str());
    return it->second();
}

bool ModuleFactory::hasModule(const std::string& moduleName) const {
    return m_creators.find(moduleName) != m_creators.end();
}

std::vector<std::string> ModuleFactory::getRegisteredModules() const {
    std::vector<std::string> names;
    names.reserve(m_creators.size());
    
    for (const auto& pair : m_creators) {
        names.push_back(pair.first);
    }
    
    return names;
}

void ModuleFactory::clear() {
    ESP_LOGW(TAG, "Clearing all %zu registered module creators", m_creators.size());
    m_creators.clear();
}

// ============================================================================
// Module Registry implementation
// ============================================================================

namespace ModuleRegistry {

esp_err_t register_all_modules() {
    ESP_LOGI(TAG, "Registering all system modules...");
    
    esp_err_t ret;
    
    // Register Logger Module (CRITICAL priority)
    {
        auto logger_module = std::make_unique<LoggerModule>();
        ret = ModuleManager::register_module(std::move(logger_module), ModuleType::CRITICAL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register Logger: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "✅ Logger registered (CRITICAL)");
    }
    
    // TODO: Register other modules when paths are fixed
    // - SensorModule (HIGH priority)
    // - ActuatorModule (STANDARD priority)
    // - ClimateModule (STANDARD priority)
    
    ESP_LOGI(TAG, "All modules registered successfully");
    return ESP_OK;
}

} // namespace ModuleRegistry

} // namespace ModESP
