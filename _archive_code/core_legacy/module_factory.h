/**
 * @file module_factory.h
 * @brief Factory for creating module instances based on manifest information
 */

#ifndef MODULE_FACTORY_H
#define MODULE_FACTORY_H

#include <memory>
#include <functional>
#include <unordered_map>
#include <string>
#include "base_module.h"

namespace ModESP {

/**
 * @brief Module creation function type
 */
using ModuleCreator = std::function<std::unique_ptr<BaseModule>()>;

/**
 * @brief Factory for creating modules dynamically
 * 
 * Modules register their creation functions with the factory,
 * allowing the ModuleManager to instantiate them based on manifest data.
 */
class ModuleFactory {
public:
    /**
     * @brief Get singleton instance
     */
    static ModuleFactory& getInstance();
    
    /**
     * @brief Register a module creator function
     * 
     * @param moduleName Name of the module (must match manifest)
     * @param creator Function that creates a new instance of the module
     * @return true if registered successfully, false if name already exists
     */
    bool registerModule(const std::string& moduleName, ModuleCreator creator);
    
    /**
     * @brief Create a module instance by name
     * 
     * @param moduleName Name of the module to create
     * @return New module instance or nullptr if not found
     */
    std::unique_ptr<BaseModule> createModule(const std::string& moduleName);
    
    /**
     * @brief Check if a module creator is registered
     * 
     * @param moduleName Name of the module
     * @return true if creator exists
     */
    bool hasModule(const std::string& moduleName) const;
    
    /**
     * @brief Get list of all registered module names
     * 
     * @return Vector of registered module names
     */
    std::vector<std::string> getRegisteredModules() const;
    
    /**
     * @brief Clear all registered creators
     * 
     * Useful for testing or system reset.
     */
    void clear();
    
private:
    ModuleFactory() = default;
    ~ModuleFactory() = default;
    
    // Prevent copying
    ModuleFactory(const ModuleFactory&) = delete;
    ModuleFactory& operator=(const ModuleFactory&) = delete;
    
    std::unordered_map<std::string, ModuleCreator> m_creators;
};

/**
 * @brief Helper macro for registering modules with the factory
 * 
 * Usage: REGISTER_MODULE(MyModule)
 * 
 * This should be placed in the module's .cpp file to automatically
 * register it with the factory on startup.
 */
#define REGISTER_MODULE(ModuleClass) \
    static bool ModuleClass##_registered = []() { \
        return ModuleFactory::getInstance().registerModule( \
            #ModuleClass, \
            []() { return std::make_unique<ModuleClass>(); } \
        ); \
    }()

/**
 * @brief Alternative registration with custom name
 * 
 * Usage: REGISTER_MODULE_AS(MyModuleImpl, "MyModule")
 */
#define REGISTER_MODULE_AS(ModuleClass, Name) \
    static bool ModuleClass##_registered = []() { \
        return ModuleFactory::getInstance().registerModule( \
            Name, \
            []() { return std::make_unique<ModuleClass>(); } \
        ); \
    }()

} // namespace ModESP

#endif // MODULE_FACTORY_H
