/**
 * @file module_manager.h
 * @brief Module lifecycle coordinator with priority-based execution
 * 
 * Manages module registration, initialization, execution, and shutdown.
 * Ensures deterministic execution order and handles module failures gracefully.
 */

#ifndef MODULE_MANAGER_H
#define MODULE_MANAGER_H

#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include "esp_err.h"
#include "base_module.h"
#include "nlohmann/json.hpp"

namespace ModuleManager {

/**
 * @brief Module execution statistics
 */
struct ModuleStats {
    const char* name;
    ModuleState state;
    ModuleType type;
    uint8_t health_score;
    uint32_t update_count;
    uint32_t error_count;
    uint32_t last_update_time_us;
    uint32_t avg_update_time_us;
    uint32_t max_update_time_us;
    uint32_t deadline_misses;
    esp_err_t last_error;
    bool enabled;
};

/**
 * @brief System health report
 */
struct HealthReport {
    size_t total_modules;
    size_t healthy_modules;
    size_t degraded_modules;
    size_t error_modules;
    size_t disabled_modules;
    uint8_t system_health_score;  // 0-100
    std::vector<ModuleStats> modules;
};

/**
 * @brief Initialize module manager
 * 
 * Must be called once during system initialization.
 * 
 * @return ESP_OK on success
 */
esp_err_t init();

/**
 * @brief Register module with priority
 * 
 * Module ownership is transferred to ModuleManager.
 * Must be called before init_modules().
 * 
 * @param module Module instance (ownership transferred)
 * @param type Module priority type
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if null or duplicate name
 */
esp_err_t register_module(std::unique_ptr<BaseModule> module, 
                         ModuleType type = ModuleType::STANDARD);

/**
 * @brief Configure all modules from JSON
 * 
 * Calls configure() on each module with its configuration section.
 * 
 * @param config Complete configuration with module sections
 */
void configure_all(const nlohmann::json& config);

/**
 * @brief Initialize all registered modules
 * 
 * Initializes modules in priority order:
 * CRITICAL → HIGH → STANDARD → LOW → BACKGROUND
 * 
 * @return ESP_OK if all critical modules initialized
 */
esp_err_t init_all();

/**
 * @brief Update all active modules
 * 
 * Called from main loop. Updates modules by priority with time tracking.
 * Handles exceptions and monitors health.
 * 
 * @param time_budget_ms Total time budget in milliseconds (default 8ms)
 */
void tick_all(uint32_t time_budget_ms = 8);

/**
 * @brief Update all modules except sensors (for multicore distribution)
 * 
 * Called from main loop on Core 0. Skips SensorModule which runs on Core 1.
 * 
 * @param time_budget_ms Total time budget in milliseconds (default 8ms)
 */
void tick_all_except_sensors(uint32_t time_budget_ms = 8);

/**
 * @brief Shutdown all modules
 * 
 * Stops modules in reverse priority order.
 * Ensures clean shutdown even if modules fail.
 */
void shutdown_all();

/**
 * @brief Find module by name
 * 
 * @param name Module name
 * @return Module pointer or nullptr if not found
 */
BaseModule* find_module(const char* name);

/**
 * @brief Get module state by name
 * 
 * @param name Module name
 * @param state Output module state
 * @return ESP_OK if found, ESP_ERR_NOT_FOUND otherwise  
 */
esp_err_t get_module_state(const char* name, ModuleState& state);

/**
 * @brief Get all modules of specific type
 * 
 * @param type Module type filter
 * @return Vector of module pointers
 */
std::vector<BaseModule*> get_modules_by_type(ModuleType type);

/**
 * @brief Get all registered modules
 * @return Vector of all module pointers
 */
std::vector<BaseModule*> get_all_modules();

/**
 * @brief Enable module
 * 
 * @param name Module name
 * @return ESP_OK if enabled, ESP_ERR_NOT_FOUND if not exists
 */
esp_err_t enable_module(const char* name);

/**
 * @brief Disable module
 * 
 * Disabled modules are skipped during tick_all().
 * 
 * @param name Module name
 * @return ESP_OK if disabled
 */
esp_err_t disable_module(const char* name);

/**
 * @brief Check if module is enabled
 * 
 * @param name Module name
 * @return true if enabled and exists
 */
bool is_module_enabled(const char* name);

/**
 * @brief Reload module
 * 
 * Performs stop → configure → init sequence.
 * 
 * @param name Module name
 * @param config New configuration (optional)
 * @return ESP_OK on success
 */
esp_err_t reload_module(const char* name, 
                       const nlohmann::json& config = nlohmann::json{});

/**
 * @brief Get module statistics
 * 
 * @param name Module name
 * @param stats Output statistics
 * @return ESP_OK if found
 */
esp_err_t get_module_stats(const char* name, ModuleStats& stats);

/**
 * @brief Get system health report
 * 
 * Aggregates health information from all modules.
 * 
 * @return Complete health report
 */
HealthReport get_health_report();

/**
 * @brief Set module update interval
 * 
 * Allows throttling module updates.
 * 0 = update every tick (default).
 * 
 * @param name Module name
 * @param interval_ms Minimum milliseconds between updates
 * @return ESP_OK on success
 */
esp_err_t set_update_interval(const char* name, uint32_t interval_ms);

/**
 * @brief Register RPC methods for all modules
 * 
 * Calls register_rpc() on each module.
 * 
 * @param registrar RPC registrar interface
 */
void register_all_rpc(IJsonRpcRegistrar& registrar);

/**
 * @brief Dump module information
 * 
 * Logs detailed information about all modules.
 * 
 * @param log_tag ESP log tag to use
 */
void dump_modules(const char* log_tag = "ModuleManager");

/**
 * @brief Module execution order callback
 * 
 * Allows customizing execution order within priority levels.
 * 
 * @param modules Modules to sort (modified in place)
 */
using ExecutionOrderCallback = std::function<void(std::vector<BaseModule*>& modules)>;

/**
 * @brief Set custom execution order
 * 
 * @param callback Order customization callback
 */
void set_execution_order(ExecutionOrderCallback callback);

/**
 * @brief Performance monitoring callback
 * 
 * Called after each module update with timing info.
 * 
 * @param module Module that was updated
 * @param update_time_us Update execution time in microseconds
 */
using PerformanceCallback = std::function<void(BaseModule* module, uint32_t update_time_us)>;

/**
 * @brief Set performance monitoring callback
 * 
 * @param callback Performance monitoring callback
 */
void set_performance_callback(PerformanceCallback callback);

} // namespace ModuleManager

#endif // MODULE_MANAGER_H
