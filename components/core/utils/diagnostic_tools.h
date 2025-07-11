/**
 * @file diagnostic_tools.h
 * @brief Diagnostic tools for detecting CPU load growth and memory leaks
 */

#ifndef DIAGNOSTIC_TOOLS_H
#define DIAGNOSTIC_TOOLS_H

#include "esp_err.h"
#include "esp_timer.h"
#include "nlohmann/json.hpp"
#include <cstdint>

namespace DiagnosticTools {

/**
 * @brief System resource snapshot
 */
struct ResourceSnapshot {
    uint64_t timestamp_us;
    size_t free_heap_bytes;
    size_t min_free_heap_bytes; 
    uint8_t cpu_usage_percent;
    size_t stack_high_water_mark;
    
    // EventBus diagnostics
    size_t eventbus_queue_size;
    size_t eventbus_total_published;
    size_t eventbus_total_processed;
    size_t eventbus_subscriptions;
    
    // SharedState diagnostics  
    size_t sharedstate_entries_used;
    size_t sharedstate_total_sets;
    size_t sharedstate_total_gets;
    size_t sharedstate_subscriptions;
    
    // ConfigManager diagnostics
    size_t config_pending_async_saves;
    size_t config_completed_saves;
    size_t config_cache_size_bytes;
    
    // Task diagnostics
    size_t total_tasks;
    size_t sensor_task_cycles;
    size_t main_loop_cycles;
};

/**
 * @brief Initialize diagnostic tools
 */
esp_err_t init();

/**
 * @brief Take a system resource snapshot
 */
ResourceSnapshot take_snapshot();

/**
 * @brief Analyze resource growth between snapshots
 * @param baseline Baseline snapshot
 * @param current Current snapshot  
 * @return Analysis results as JSON
 */
nlohmann::json analyze_resource_growth(const ResourceSnapshot& baseline, 
                                      const ResourceSnapshot& current);

/**
 * @brief Detect potential memory leaks
 * @return JSON report of potential leaks
 */
nlohmann::json detect_memory_leaks();

/**
 * @brief Detect EventBus issues
 * @return JSON report of EventBus problems
 */
nlohmann::json detect_eventbus_issues();

/**
 * @brief Detect ConfigManager async issues
 * @return JSON report of async config problems
 */
nlohmann::json detect_config_async_issues();

/**
 * @brief Detect task/CPU load issues
 * @return JSON report of task problems
 */
nlohmann::json detect_task_issues();

/**
 * @brief Generate comprehensive diagnostic report
 * @return Full system diagnostic report
 */
nlohmann::json generate_diagnostic_report();

/**
 * @brief Start continuous monitoring (logs issues automatically)
 * @param check_interval_ms How often to check (default 5000ms)
 */
esp_err_t start_continuous_monitoring(uint32_t check_interval_ms = 5000);

/**
 * @brief Stop continuous monitoring
 */
void stop_continuous_monitoring();

/**
 * @brief Force garbage collection and cleanup
 * @return Number of bytes freed
 */
size_t force_system_cleanup();

} // namespace DiagnosticTools

#endif // DIAGNOSTIC_TOOLS_H 