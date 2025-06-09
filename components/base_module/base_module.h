/**
 * @file base_module.h
 * @brief Base interface for all ModuChill modules
 * 
 * Defines the contract that all modules must implement.
 * Modules are the building blocks of the ModuChill system.
 */

#ifndef BASE_MODULE_H
#define BASE_MODULE_H

#include <stdint.h>
#include "esp_err.h"
#include "nlohmann/json.hpp"

// Forward declarations
class IJsonRpcRegistrar;

/**
 * @brief Module states
 */
enum class ModuleState {
    CREATED,      // Module created but not configured
    CONFIGURED,   // Module configured but not initialized
    INITIALIZED,  // Module initialized and ready to run
    RUNNING,      // Module is actively running
    ERROR,        // Module encountered an error
    STOPPED       // Module has been stopped
};

/**
 * @brief Base interface for all system modules
 * 
 * All modules must inherit from this class and implement the pure virtual methods.
 * Modules communicate through EventBus and SharedState, not directly.
 */
class BaseModule {
public:
    virtual ~BaseModule() = default;

    // === Required methods (must implement) ===
    
    /**
     * @brief Get module name for identification
     * @return Unique module name (e.g., "climate_control", "sensor_manager")
     */
    virtual const char* get_name() const = 0;
    
    /**
     * @brief Initialize module after configuration
     * @return ESP_OK on success, error code otherwise
     */
    virtual esp_err_t init() = 0;
    
    /**
     * @brief Update module state (called from main loop)
     * Should complete within module's time budget (see get_max_update_time_us)
     */
    virtual void update() = 0;
    
    /**
     * @brief Stop module and cleanup resources
     */
    virtual void stop() = 0;

    // === Optional methods (have default implementation) ===
    
    /**
     * @brief Configure module with JSON configuration
     * @param config Module-specific configuration section
     */
    virtual void configure(const nlohmann::json& config) {}
    
    /**
     * @brief Check if module is healthy
     * @return true if module is functioning correctly
     */
    virtual bool is_healthy() const { return true; }
    
    /**
     * @brief Get module health score
     * @return Health score 0-100 (100 = perfect health)
     */
    virtual uint8_t get_health_score() const { return 100; }
    
    /**
     * @brief Get maximum allowed update time in microseconds
     * @return Maximum time update() should take
     */
    virtual uint32_t get_max_update_time_us() const { return 2000; } // 2ms default
    
    /**
     * @brief Register module's RPC methods
     * @param rpc RPC registrar interface
     */
    virtual void register_rpc(IJsonRpcRegistrar& rpc) {}
    
    // Примітка: Управління станом модуля здійснюється виключно через ModuleManager
    // Модулі не мають прямого доступу до зміни свого стану
};

/**
 * @brief Module priority types for execution order
 */
enum class ModuleType {
    CRITICAL = 0,    // Safety & protection (< 100μs)
    HIGH = 1,        // Real-time I/O (< 500μs)  
    STANDARD = 2,    // Business logic (< 2ms)
    LOW = 3,         // UI & display (< 5ms)
    BACKGROUND = 4   // Analytics & logging (< 10ms)
};

#endif // BASE_MODULE_H
