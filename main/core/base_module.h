#pragma once

#include <esp_err.h>
#include <cstdint>
#include <nlohmann/json.hpp>

// Forward declarations
class IJsonRpcRegistrar;

namespace ModuChill {

using json = nlohmann::json;

/**
 * Base interface for all ModuChill modules
 * 
 * Each module must implement this interface to be managed by ModuleManager.
 * Modules should follow single responsibility principle and communicate
 * through EventBus and SharedState.
 */
class BaseModule {
public:
    virtual ~BaseModule() = default;

    // ===== Required Methods =====
    
    /**
     * Get unique module name
     * @return Module identifier (e.g., "climate_control", "sensor_manager")
     */
    virtual const char* get_name() const = 0;
    
    /**
     * Initialize module
     * Called once during system startup in priority order.
     * Should be fast (<100ms) and fail gracefully.
     * @return ESP_OK on success, error code on failure
     */
    virtual esp_err_t init() = 0;
    
    /**
     * Update module state
     * Called from main loop at configured interval.
     * Must be non-blocking and fast (<2ms typical).
     */
    virtual void update() = 0;
    
    /**
     * Stop module
     * Called during shutdown in reverse priority order.
     * Should release all resources and stop gracefully.
     */
    virtual void stop() = 0;
    
    // ===== Optional Methods =====
    
    /**
     * Configure module from JSON
     * Called before init() with module-specific configuration.
     * @param config Module configuration object
     */
    virtual void configure(const json& config) {}
    
    /**
     * Check if module is healthy
     * @return true if operating normally, false if degraded
     */
    virtual bool is_healthy() const { return true; }
    
    /**
     * Get module health score
     * @return Health score 0-100 (100 = perfect health)
     */
    virtual uint8_t get_health_score() const { return 100; }
    
    /**
     * Get maximum allowed update time
     * @return Maximum microseconds update() should take
     */
    virtual uint32_t get_max_update_time_us() const { return 2000; } // 2ms default
    
    /**
     * Register RPC methods
     * Called during init to register module's RPC API.
     * @param registrar RPC registrar interface
     */
    virtual void register_rpc(IJsonRpcRegistrar& registrar) {}
    
    /**
     * Get update interval
     * @return Milliseconds between update() calls (0 = every tick)
     */
    virtual uint32_t get_update_interval_ms() const { return 0; }
};

} // namespace ModuChill