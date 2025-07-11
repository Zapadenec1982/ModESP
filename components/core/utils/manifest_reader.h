/**
 * @file manifest_reader.h
 * @brief Runtime access to manifest-generated data
 * 
 * Provides a unified interface for accessing module information,
 * API registrations, and UI schemas generated from manifests.
 */

#ifndef MANIFEST_READER_H
#define MANIFEST_READER_H

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "esp_err.h"
#include "base_module.h"
#include "generated_module_info.h"
#include "generated_api_registry.h"
#include "generated_ui_schemas.h"

// Forward declarations
class IJsonRpcRegistrar;

namespace ModESP {

/**
 * @brief API access levels
 */
enum class AccessLevel {
    USER = 0,
    TECHNICIAN = 1,
    ADMIN = 2
};

/**
 * @brief API method information from manifests
 */
struct APIMethodInfo {
    const char* method;         // Full method name (e.g., "sensor.get_temperature")
    const char* module;         // Module providing this API
    const char* description;    // Method description
    AccessLevel access_level;   // Required access level
};

/**
 * @brief UI schema information
 */
struct UISchema {
    const char* module_name;
    // Additional UI fields will be added as needed
};

// External data from generated files
extern const APIMethodInfo generated_apis[];
extern const size_t generated_api_count;
extern const UISchema generated_ui_schemas[];
extern const size_t generated_ui_schema_count;

// Function to register generated APIs
extern void register_generated_apis(IJsonRpcRegistrar& registrar);

/**
 * @brief Interface for accessing module manifest data
 */
class IModuleManifest {
public:
    virtual ~IModuleManifest() = default;
    
    virtual const char* getName() const = 0;
    virtual const char* getVersion() const = 0;
    virtual const char* getDescription() const = 0;
    virtual ModuleType getType() const = 0;
    virtual Priority getPriority() const = 0;
    virtual std::vector<std::string> getDependencies() const = 0;
    virtual const char* getConfigFile() const = 0;
    virtual bool hasAPI(const std::string& apiName) const = 0;
    virtual const UISchema* getUISchema() const = 0;
};

/**
 * @brief Implementation of IModuleManifest using generated data
 */
class GeneratedModuleManifest : public IModuleManifest {
public:
    GeneratedModuleManifest(const ModuleInfo* info) : m_info(info) {}
    
    const char* getName() const override { return m_info->name; }
    const char* getVersion() const override { return m_info->version; }
    const char* getDescription() const override { return m_info->description; }
    ModuleType getType() const override { return m_info->type; }
    Priority getPriority() const override { return m_info->priority; }
    
    std::vector<std::string> getDependencies() const override {
        std::vector<std::string> deps;
        for (const auto& dep : m_info->dependencies) {
            deps.push_back(dep);
        }
        return deps;
    }
    
    const char* getConfigFile() const override { return m_info->config_file; }
    
    bool hasAPI(const std::string& apiName) const override;
    const UISchema* getUISchema() const override;
    
private:
    const ModuleInfo* m_info;
};

/**
 * @brief Singleton class for reading manifest-generated data
 */
class ManifestReader {
public:
    /**
     * @brief Get singleton instance
     */
    static ManifestReader& getInstance();
    
    /**
     * @brief Initialize manifest reader
     * 
     * Loads all generated data and builds indexes.
     * Should be called early in system initialization.
     * 
     * @return ESP_OK on success
     */
    esp_err_t init();
    
    /**
     * @brief Get module manifest by name
     * 
     * @param moduleName Module name
     * @return Module manifest or nullptr if not found
     */
    std::shared_ptr<IModuleManifest> getModuleManifest(const std::string& moduleName);
    
    /**
     * @brief Get all module manifests
     * 
     * @return Vector of all module manifests
     */
    std::vector<std::shared_ptr<IModuleManifest>> getAllModuleManifests();
    
    /**
     * @brief Get modules by type
     * 
     * @param type Module type filter
     * @return Vector of matching module manifests
     */
    std::vector<std::shared_ptr<IModuleManifest>> getModulesByType(ModuleType type);
    
    /**
     * @brief Get modules by priority
     * 
     * @param priority Priority filter
     * @return Vector of matching module manifests
     */
    std::vector<std::shared_ptr<IModuleManifest>> getModulesByPriority(Priority priority);
    
    /**
     * @brief Check if module has dependency
     * 
     * @param moduleName Module to check
     * @param dependency Dependency name
     * @return true if module depends on dependency
     */
    bool hasDependency(const std::string& moduleName, const std::string& dependency);
    
    /**
     * @brief Get modules that depend on a given module
     * 
     * @param moduleName Module name
     * @return Vector of dependent module names
     */
    std::vector<std::string> getDependentModules(const std::string& moduleName);
    
    /**
     * @brief Validate module dependencies
     * 
     * Checks if all dependencies for a module are available.
     * 
     * @param moduleName Module to validate
     * @param missingDeps Output vector of missing dependencies
     * @return ESP_OK if all dependencies satisfied
     */
    esp_err_t validateDependencies(const std::string& moduleName, 
                                   std::vector<std::string>& missingDeps);
    
    /**
     * @brief Get topologically sorted module load order
     * 
     * Sorts modules based on dependencies to ensure proper initialization order.
     * 
     * @param sortedModules Output vector of module names in load order
     * @return ESP_OK on success, ESP_ERR_INVALID_STATE if circular dependency
     */
    esp_err_t getModuleLoadOrder(std::vector<std::string>& sortedModules);
    
    /**
     * @brief Register all generated APIs
     * 
     * Calls the generated register_generated_apis() function.
     * Should be called after RPC system initialization.
     * 
     * @param registrar RPC registrar interface
     * @return ESP_OK on success
     */
    esp_err_t registerGeneratedAPIs(IJsonRpcRegistrar& registrar);
    
    /**
     * @brief Get UI schema for module
     * 
     * @param moduleName Module name
     * @return UI schema or nullptr if not found
     */
    const UISchema* getUISchema(const std::string& moduleName);
    
    /**
     * @brief Get all available API methods
     * 
     * @return Vector of all API method names
     */
    std::vector<std::string> getAllAPIMethods();
    
    /**
     * @brief Get API info by method name
     * 
     * @param methodName API method name
     * @return API info or nullptr if not found
     */
    const APIMethodInfo* getAPIInfo(const std::string& methodName);
    
    /**
     * @brief Convert priority enum to execution priority
     * 
     * Maps manifest Priority to ModuleType for compatibility with existing system.
     * 
     * @param priority Manifest priority
     * @return Corresponding ModuleType
     */
    static ::ModuleType priorityToModuleType(Priority priority);
    
    /**
     * @brief Dump manifest information for debugging
     * 
     * @param tag Log tag to use
     */
    void dumpManifestInfo(const char* tag = "ManifestReader");
    
private:
    ManifestReader() = default;
    ~ManifestReader() = default;
    
    // Prevent copying
    ManifestReader(const ManifestReader&) = delete;
    ManifestReader& operator=(const ManifestReader&) = delete;
    
    bool m_initialized = false;
    std::unordered_map<std::string, std::shared_ptr<GeneratedModuleManifest>> m_moduleMap;
    std::unordered_map<std::string, std::vector<std::string>> m_dependencyGraph;
    std::unordered_map<std::string, const APIMethodInfo*> m_apiMap;
};

} // namespace ModESP

#endif // MANIFEST_READER_H
