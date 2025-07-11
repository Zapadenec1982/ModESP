/**
 * @file manifest_reader.cpp
 * @brief Implementation of manifest reader
 */

#include "manifest_reader.h"
#include "esp_log.h"
#include <algorithm>
#include <queue>
#include <set>
#include <cstring>

static const char* TAG = "ManifestReader";

namespace ModESP {

// Implementation of GeneratedModuleManifest methods
bool GeneratedModuleManifest::hasAPI(const std::string& apiName) const {
    // Get all APIs and check if this module provides the requested API
    auto& reader = ManifestReader::getInstance();
    const auto* apiInfo = reader.getAPIInfo(apiName);
    
    if (apiInfo && apiInfo->module) {
        return strcmp(apiInfo->module, m_info->name) == 0;
    }
    return false;
}

const UISchema* GeneratedModuleManifest::getUISchema() const {
    // Access UI schema through ManifestReader
    auto& reader = ManifestReader::getInstance();
    return reader.getUISchema(m_info->name);
}

// ManifestReader implementation
ManifestReader& ManifestReader::getInstance() {
    static ManifestReader instance;
    return instance;
}

esp_err_t ManifestReader::init() {
    if (m_initialized) {
        ESP_LOGW(TAG, "ManifestReader already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing ManifestReader with %zu modules", generated_module_count);
    
    // Load all module manifests
    for (size_t i = 0; i < generated_module_count; i++) {
        const ModuleInfo* info = &generated_modules[i];
        auto manifest = std::make_shared<GeneratedModuleManifest>(info);
        m_moduleMap[info->name] = manifest;
        
        // Build dependency graph
        for (const auto& dep : info->dependencies) {
            m_dependencyGraph[dep].push_back(info->name);
        }
        
        ESP_LOGD(TAG, "Loaded module: %s v%s (type=%d, priority=%d)", 
                 info->name, info->version, 
                 static_cast<int>(info->type), 
                 static_cast<int>(info->priority));
    }
    
    // Load API registry
    for (size_t i = 0; i < generated_api_count; i++) {
        const APIMethodInfo* api = &generated_apis[i];
        m_apiMap[api->method] = api;
        ESP_LOGD(TAG, "Loaded API: %s (module=%s, access=%d)", 
                 api->method, api->module, static_cast<int>(api->access_level));
    }
    
    m_initialized = true;
    ESP_LOGI(TAG, "ManifestReader initialized successfully");
    return ESP_OK;
}

std::shared_ptr<IModuleManifest> ManifestReader::getModuleManifest(const std::string& moduleName) {
    auto it = m_moduleMap.find(moduleName);
    if (it != m_moduleMap.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<IModuleManifest>> ManifestReader::getAllModuleManifests() {
    std::vector<std::shared_ptr<IModuleManifest>> result;
    for (const auto& pair : m_moduleMap) {
        result.push_back(pair.second);
    }
    return result;
}

std::vector<std::shared_ptr<IModuleManifest>> ManifestReader::getModulesByType(ModuleType type) {
    std::vector<std::shared_ptr<IModuleManifest>> result;
    for (const auto& pair : m_moduleMap) {
        if (pair.second->getType() == type) {
            result.push_back(pair.second);
        }
    }
    return result;
}

std::vector<std::shared_ptr<IModuleManifest>> ManifestReader::getModulesByPriority(Priority priority) {
    std::vector<std::shared_ptr<IModuleManifest>> result;
    for (const auto& pair : m_moduleMap) {
        if (pair.second->getPriority() == priority) {
            result.push_back(pair.second);
        }
    }
    return result;
}

bool ManifestReader::hasDependency(const std::string& moduleName, const std::string& dependency) {
    auto manifest = getModuleManifest(moduleName);
    if (!manifest) {
        return false;
    }
    
    auto deps = manifest->getDependencies();
    return std::find(deps.begin(), deps.end(), dependency) != deps.end();
}

std::vector<std::string> ManifestReader::getDependentModules(const std::string& moduleName) {
    auto it = m_dependencyGraph.find(moduleName);
    if (it != m_dependencyGraph.end()) {
        return it->second;
    }
    return {};
}

esp_err_t ManifestReader::validateDependencies(const std::string& moduleName, 
                                               std::vector<std::string>& missingDeps) {
    missingDeps.clear();
    
    auto manifest = getModuleManifest(moduleName);
    if (!manifest) {
        ESP_LOGE(TAG, "Module %s not found", moduleName.c_str());
        return ESP_ERR_NOT_FOUND;
    }
    
    auto deps = manifest->getDependencies();
    for (const auto& dep : deps) {
        // Check if dependency is a module
        bool found = (m_moduleMap.find(dep) != m_moduleMap.end());
        
        // Special handling for system modules (ESPhal, SharedState, EventBus)
        // These are provided by the system, not through manifests
        const std::set<std::string> systemModules = {
            "ESPhal", "SharedState", "EventBus", "ModuleManager"
        };
        
        if (!found && systemModules.find(dep) == systemModules.end()) {
            missingDeps.push_back(dep);
        }
    }
    
    if (!missingDeps.empty()) {
        ESP_LOGW(TAG, "Module %s has %zu missing dependencies", 
                 moduleName.c_str(), missingDeps.size());
        return ESP_ERR_NOT_FOUND;
    }
    
    return ESP_OK;
}

esp_err_t ManifestReader::getModuleLoadOrder(std::vector<std::string>& sortedModules) {
    sortedModules.clear();
    
    // Build in-degree map for topological sort
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adjList;
    
    // Initialize all modules with 0 in-degree
    for (const auto& pair : m_moduleMap) {
        inDegree[pair.first] = 0;
    }
    
    // Build adjacency list and calculate in-degrees
    for (const auto& pair : m_moduleMap) {
        auto deps = pair.second->getDependencies();
        for (const auto& dep : deps) {
            // Only consider dependencies that are actual modules
            if (m_moduleMap.find(dep) != m_moduleMap.end()) {
                adjList[dep].push_back(pair.first);
                inDegree[pair.first]++;
            }
        }
    }
    
    // Kahn's algorithm for topological sort
    std::queue<std::string> queue;
    
    // Start with modules that have no dependencies
    for (const auto& pair : inDegree) {
        if (pair.second == 0) {
            queue.push(pair.first);
        }
    }
    
    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();
        sortedModules.push_back(current);
        
        // Process all modules that depend on current
        for (const auto& dependent : adjList[current]) {
            inDegree[dependent]--;
            if (inDegree[dependent] == 0) {
                queue.push(dependent);
            }
        }
    }
    
    // Check for circular dependencies
    if (sortedModules.size() != m_moduleMap.size()) {
        ESP_LOGE(TAG, "Circular dependency detected! Sorted %zu of %zu modules", 
                 sortedModules.size(), m_moduleMap.size());
        return ESP_ERR_INVALID_STATE;
    }
    
    // Sort modules within same dependency level by priority
    std::stable_sort(sortedModules.begin(), sortedModules.end(),
        [this](const std::string& a, const std::string& b) {
            auto manifestA = getModuleManifest(a);
            auto manifestB = getModuleManifest(b);
            if (manifestA && manifestB) {
                return manifestA->getPriority() < manifestB->getPriority();
            }
            return false;
        });
    
    return ESP_OK;
}

esp_err_t ManifestReader::registerGeneratedAPIs(IJsonRpcRegistrar& registrar) {
    ESP_LOGI(TAG, "Registering %zu generated APIs", generated_api_count);
    
    // Call the generated registration function
    register_generated_apis(registrar);
    
    return ESP_OK;
}

const UISchema* ManifestReader::getUISchema(const std::string& moduleName) {
    // Find UI schema for the module
    for (size_t i = 0; i < generated_ui_schema_count; i++) {
        if (strcmp(generated_ui_schemas[i].module_name, moduleName.c_str()) == 0) {
            return &generated_ui_schemas[i];
        }
    }
    return nullptr;
}

std::vector<std::string> ManifestReader::getAllAPIMethods() {
    std::vector<std::string> methods;
    for (const auto& pair : m_apiMap) {
        methods.push_back(pair.first);
    }
    return methods;
}

const APIMethodInfo* ManifestReader::getAPIInfo(const std::string& methodName) {
    auto it = m_apiMap.find(methodName);
    if (it != m_apiMap.end()) {
        return it->second;
    }
    return nullptr;
}

::ModuleType ManifestReader::priorityToModuleType(Priority priority) {
    switch (priority) {
        case Priority::CRITICAL:
            return ::ModuleType::CRITICAL;
        case Priority::HIGH:
            return ::ModuleType::HIGH;
        case Priority::NORMAL:
            return ::ModuleType::STANDARD;
        case Priority::LOW:
            return ::ModuleType::LOW;
        default:
            return ::ModuleType::STANDARD;
    }
}

void ManifestReader::dumpManifestInfo(const char* tag) {
    ESP_LOGI(tag, "=== Manifest Information ===");
    ESP_LOGI(tag, "Total modules: %zu", m_moduleMap.size());
    ESP_LOGI(tag, "Total APIs: %zu", m_apiMap.size());
    
    // Dump modules by type
    ESP_LOGI(tag, "\nModules by type:");
    for (int type = 0; type <= 2; type++) {
        auto modules = getModulesByType(static_cast<ModuleType>(type));
        if (!modules.empty()) {
            ESP_LOGI(tag, "  Type %d: %zu modules", type, modules.size());
            for (const auto& m : modules) {
                ESP_LOGI(tag, "    - %s v%s", m->getName(), m->getVersion());
            }
        }
    }
    
    // Dump dependency graph
    ESP_LOGI(tag, "\nDependency graph:");
    for (const auto& pair : m_dependencyGraph) {
        if (!pair.second.empty()) {
            ESP_LOGI(tag, "  %s is required by:", pair.first.c_str());
            for (const auto& dep : pair.second) {
                ESP_LOGI(tag, "    - %s", dep.c_str());
            }
        }
    }
    
    // Get and dump load order
    std::vector<std::string> loadOrder;
    if (getModuleLoadOrder(loadOrder) == ESP_OK) {
        ESP_LOGI(tag, "\nModule load order:");
        for (size_t i = 0; i < loadOrder.size(); i++) {
            auto manifest = getModuleManifest(loadOrder[i]);
            if (manifest) {
                ESP_LOGI(tag, "  %zu. %s (priority=%d)", 
                         i + 1, loadOrder[i].c_str(), 
                         static_cast<int>(manifest->getPriority()));
            }
        }
    }
}

} // namespace ModESP
