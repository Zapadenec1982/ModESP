/**
 * @file module_factory.cpp
 * @brief Implementation of module factory
 */

#include "module_factory.h"
#include "esp_log.h"

static const char* TAG = "ModuleFactory";

namespace ModESP {

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

} // namespace ModESP
