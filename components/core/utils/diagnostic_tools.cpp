/**
 * @file diagnostic_tools.cpp
 * @brief Implementation of diagnostic tools for CPU load and memory leak detection
 */

#include "diagnostic_tools.h"
#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config_manager.h"
#include "config_manager_async.h"
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/task.h>
#include <algorithm>
#include <vector>

static const char* TAG = "DiagnosticTools";

namespace DiagnosticTools {

// Internal state
static bool monitoring_active = false;
static TaskHandle_t monitor_task_handle = nullptr;
static ResourceSnapshot baseline_snapshot;
static std::vector<ResourceSnapshot> history;
static uint32_t monitor_interval_ms = 5000;

// Forward declarations
static void monitor_task(void* param);
static size_t estimate_json_memory_usage();
static size_t count_freertos_tasks();

esp_err_t init() {
    ESP_LOGI(TAG, "Initializing diagnostic tools");
    
    // Reserve memory for history tracking
    history.reserve(100); // Keep last 100 snapshots
    
    // Take initial baseline
    baseline_snapshot = take_snapshot();
    
    ESP_LOGI(TAG, "Diagnostic tools initialized, baseline CPU: %u%%, heap: %zu KB",
             baseline_snapshot.cpu_usage_percent, 
             baseline_snapshot.free_heap_bytes / 1024);
    
    return ESP_OK;
}

ResourceSnapshot take_snapshot() {
    ResourceSnapshot snapshot = {};
    
    // Basic system metrics
    snapshot.timestamp_us = esp_timer_get_time();
    snapshot.free_heap_bytes = Application::get_free_heap();
    snapshot.min_free_heap_bytes = Application::get_min_free_heap();
    snapshot.cpu_usage_percent = Application::get_cpu_usage();
    snapshot.stack_high_water_mark = Application::get_stack_high_water_mark();
    
    // EventBus metrics
    auto eventbus_stats = EventBus::get_stats();
    snapshot.eventbus_queue_size = eventbus_stats.queue_size;
    snapshot.eventbus_total_published = eventbus_stats.total_published;
    snapshot.eventbus_total_processed = eventbus_stats.total_processed;
    snapshot.eventbus_subscriptions = eventbus_stats.total_subscriptions;
    
    // SharedState metrics
    auto sharedstate_stats = SharedState::get_stats();
    snapshot.sharedstate_entries_used = sharedstate_stats.used;
    snapshot.sharedstate_total_sets = sharedstate_stats.total_sets;
    snapshot.sharedstate_total_gets = sharedstate_stats.total_gets;
    snapshot.sharedstate_subscriptions = sharedstate_stats.subscriptions;
    
    // ConfigManager metrics
#ifdef CONFIG_USE_ASYNC_SAVE
    auto async_stats = ConfigManagerAsync::get_async_stats();
    snapshot.config_pending_async_saves = async_stats.pending_saves;
    snapshot.config_completed_saves = async_stats.completed_saves;
#else
    snapshot.config_pending_async_saves = 0;
    snapshot.config_completed_saves = 0;
#endif
    
    // Estimate config cache size
    snapshot.config_cache_size_bytes = estimate_json_memory_usage();
    
    // Task metrics
    snapshot.total_tasks = count_freertos_tasks();
    snapshot.sensor_task_cycles = 0; // TODO: Get from Application if available
    snapshot.main_loop_cycles = 0; // TODO: Get from Application::get_system_diagnostics()
    
    return snapshot;
}

nlohmann::json analyze_resource_growth(const ResourceSnapshot& baseline, 
                                      const ResourceSnapshot& current) {
    nlohmann::json analysis;
    
    // Time elapsed
    uint64_t elapsed_us = current.timestamp_us - baseline.timestamp_us;
    double elapsed_hours = elapsed_us / (1000000.0 * 3600.0);
    
    analysis["elapsed_hours"] = elapsed_hours;
    analysis["timestamp"] = current.timestamp_us;
    
    // Memory analysis
    int32_t heap_delta = (int32_t)current.free_heap_bytes - (int32_t)baseline.free_heap_bytes;
    analysis["memory"]["heap_delta_bytes"] = heap_delta;
    analysis["memory"]["heap_delta_per_hour"] = elapsed_hours > 0 ? heap_delta / elapsed_hours : 0;
    analysis["memory"]["current_free_kb"] = current.free_heap_bytes / 1024;
    analysis["memory"]["baseline_free_kb"] = baseline.free_heap_bytes / 1024;
    
    // CPU analysis
    int32_t cpu_delta = (int32_t)current.cpu_usage_percent - (int32_t)baseline.cpu_usage_percent;
    analysis["cpu"]["usage_delta_percent"] = cpu_delta;
    analysis["cpu"]["usage_delta_per_hour"] = elapsed_hours > 0 ? cpu_delta / elapsed_hours : 0;
    analysis["cpu"]["current_percent"] = current.cpu_usage_percent;
    analysis["cpu"]["baseline_percent"] = baseline.cpu_usage_percent;
    
    // EventBus analysis
    uint64_t eventbus_ops_delta = (current.eventbus_total_published + current.eventbus_total_processed) -
                                  (baseline.eventbus_total_published + baseline.eventbus_total_processed);
    analysis["eventbus"]["total_operations_delta"] = eventbus_ops_delta;
    analysis["eventbus"]["queue_size_current"] = current.eventbus_queue_size;
    analysis["eventbus"]["subscriptions_delta"] = (int32_t)current.eventbus_subscriptions - (int32_t)baseline.eventbus_subscriptions;
    
    // SharedState analysis
    uint64_t sharedstate_ops_delta = (current.sharedstate_total_sets + current.sharedstate_total_gets) -
                                     (baseline.sharedstate_total_sets + baseline.sharedstate_total_gets);
    analysis["shared_state"]["total_operations_delta"] = sharedstate_ops_delta;
    analysis["shared_state"]["entries_delta"] = (int32_t)current.sharedstate_entries_used - (int32_t)baseline.sharedstate_entries_used;
    analysis["shared_state"]["subscriptions_delta"] = (int32_t)current.sharedstate_subscriptions - (int32_t)baseline.sharedstate_subscriptions;
    
    // ConfigManager analysis
    analysis["config_manager"]["pending_saves_current"] = current.config_pending_async_saves;
    analysis["config_manager"]["completed_saves_delta"] = current.config_completed_saves - baseline.config_completed_saves;
    analysis["config_manager"]["cache_size_delta_bytes"] = (int32_t)current.config_cache_size_bytes - (int32_t)baseline.config_cache_size_bytes;
    
    // Task analysis
    analysis["tasks"]["total_delta"] = (int32_t)current.total_tasks - (int32_t)baseline.total_tasks;
    analysis["tasks"]["main_loop_cycles_delta"] = current.main_loop_cycles - baseline.main_loop_cycles;
    
    // Risk assessment
    bool memory_leak_risk = heap_delta < -1024 && elapsed_hours > 1; // >1KB lost per hour
    bool cpu_growth_risk = cpu_delta > 5 && elapsed_hours > 1; // >5% growth per hour
    bool eventbus_growth_risk = current.eventbus_queue_size > 10;
    bool config_backlog_risk = current.config_pending_async_saves > 5;
    
    analysis["risk_assessment"]["memory_leak"] = memory_leak_risk;
    analysis["risk_assessment"]["cpu_growth"] = cpu_growth_risk;
    analysis["risk_assessment"]["eventbus_overload"] = eventbus_growth_risk;
    analysis["risk_assessment"]["config_backlog"] = config_backlog_risk;
    analysis["risk_assessment"]["overall_risk"] = memory_leak_risk || cpu_growth_risk || eventbus_growth_risk || config_backlog_risk;
    
    return analysis;
}

nlohmann::json detect_memory_leaks() {
    nlohmann::json report;
    
    size_t current_free = esp_get_free_heap_size();
    size_t min_free = esp_get_minimum_free_heap_size();
    
    // Check heap fragmentation
    multi_heap_info_t heap_info;
    heap_caps_get_info(&heap_info, MALLOC_CAP_DEFAULT);
    
    report["heap"]["current_free_bytes"] = current_free;
    report["heap"]["minimum_free_bytes"] = min_free;
    report["heap"]["largest_free_block"] = heap_info.largest_free_block;
    report["heap"]["total_allocated"] = heap_info.total_allocated_bytes;
    report["heap"]["fragmentation_percent"] = 
        current_free > 0 ? ((double)(current_free - heap_info.largest_free_block) / current_free) * 100.0 : 0;
    
    // Potential leak indicators
    bool fragmentation_high = (current_free > 0) && 
                             (((double)(current_free - heap_info.largest_free_block) / current_free) > 0.3);
    bool heap_declining = min_free < (current_free * 0.8);
    
    report["indicators"]["high_fragmentation"] = fragmentation_high;
    report["indicators"]["heap_declining"] = heap_declining;
    report["indicators"]["potential_leak"] = fragmentation_high || heap_declining;
    
    // Check specific components for memory usage
    report["json_estimated_usage_bytes"] = estimate_json_memory_usage();
    
    return report;
}

nlohmann::json detect_eventbus_issues() {
    nlohmann::json report;
    
    auto stats = EventBus::get_stats();
    
    report["queue_size"] = stats.queue_size;
    report["max_queue_size"] = stats.max_queue_size;
    report["total_published"] = stats.total_published;
    report["total_processed"] = stats.total_processed;
    report["total_dropped"] = stats.total_dropped;
    report["subscriptions"] = stats.total_subscriptions;
    report["avg_process_time_us"] = stats.avg_process_time_us;
    
    // Issue detection
    bool queue_backlog = stats.queue_size > (stats.max_queue_size / 2);
    bool high_drop_rate = stats.total_dropped > (stats.total_published * 0.05); // >5% drop rate
    bool slow_processing = stats.avg_process_time_us > 1000; // >1ms avg
    bool too_many_subscriptions = stats.total_subscriptions > 50;
    
    report["issues"]["queue_backlog"] = queue_backlog;
    report["issues"]["high_drop_rate"] = high_drop_rate;
    report["issues"]["slow_processing"] = slow_processing;
    report["issues"]["too_many_subscriptions"] = too_many_subscriptions;
    report["issues"]["has_issues"] = queue_backlog || high_drop_rate || slow_processing || too_many_subscriptions;
    
    // Performance ratios
    report["performance"]["process_success_rate"] = 
        stats.total_published > 0 ? (double)stats.total_processed / stats.total_published : 1.0;
    report["performance"]["queue_utilization"] = 
        stats.max_queue_size > 0 ? (double)stats.queue_size / stats.max_queue_size : 0.0;
    
    return report;
}

nlohmann::json detect_config_async_issues() {
    nlohmann::json report;
    
#ifdef CONFIG_USE_ASYNC_SAVE
    auto stats = ConfigManagerAsync::get_async_stats();
    
    report["pending_saves"] = stats.pending_saves;
    report["completed_saves"] = stats.completed_saves;
    report["failed_saves"] = stats.failed_saves;
    report["total_write_time_ms"] = stats.total_write_time_ms;
    report["max_write_time_ms"] = stats.max_write_time_ms;
    report["total_bytes_written"] = stats.total_bytes_written;
    
    // Issue detection
    bool pending_backlog = stats.pending_saves > 5;
    bool high_failure_rate = (stats.completed_saves + stats.failed_saves) > 0 &&
                            ((double)stats.failed_saves / (stats.completed_saves + stats.failed_saves)) > 0.1;
    bool slow_writes = stats.max_write_time_ms > 5000; // >5 seconds
    
    report["issues"]["pending_backlog"] = pending_backlog;
    report["issues"]["high_failure_rate"] = high_failure_rate;  
    report["issues"]["slow_writes"] = slow_writes;
    report["issues"]["has_issues"] = pending_backlog || high_failure_rate || slow_writes;
    
    // Performance metrics
    if (stats.completed_saves > 0) {
        report["performance"]["avg_write_time_ms"] = stats.total_write_time_ms / stats.completed_saves;
        report["performance"]["success_rate"] = 
            (double)stats.completed_saves / (stats.completed_saves + stats.failed_saves);
    }
#else
    report["async_disabled"] = true;
    report["issues"]["has_issues"] = false;
#endif
    
    return report;
}

nlohmann::json detect_task_issues() {
    nlohmann::json report;
    
    // Task count
    size_t task_count = count_freertos_tasks();
    report["total_tasks"] = task_count;
    
    // Stack usage analysis
    size_t stack_remaining = Application::get_stack_high_water_mark();
    report["main_stack_remaining"] = stack_remaining;
    
    // CPU usage
    uint8_t cpu_usage = Application::get_cpu_usage();
    report["cpu_usage_percent"] = cpu_usage;
    
    // Issue detection
    bool too_many_tasks = task_count > 20;
    bool low_stack = stack_remaining < 2048;
    bool high_cpu = cpu_usage > 80;
    
    report["issues"]["too_many_tasks"] = too_many_tasks;
    report["issues"]["low_stack"] = low_stack;
    report["issues"]["high_cpu"] = high_cpu;
    report["issues"]["has_issues"] = too_many_tasks || low_stack || high_cpu;
    
    return report;
}

nlohmann::json generate_diagnostic_report() {
    ESP_LOGI(TAG, "Generating comprehensive diagnostic report");
    
    nlohmann::json report;
    ResourceSnapshot current = take_snapshot();
    
    report["timestamp"] = current.timestamp_us;
    report["uptime_ms"] = Application::get_uptime_ms();
    
    // Current system state
    report["current_state"] = {
        {"cpu_percent", current.cpu_usage_percent},
        {"free_heap_kb", current.free_heap_bytes / 1024},
        {"min_free_heap_kb", current.min_free_heap_bytes / 1024},
        {"stack_remaining", current.stack_high_water_mark},
        {"total_tasks", current.total_tasks}
    };
    
    // Component diagnostics
    report["memory_leaks"] = detect_memory_leaks();
    report["eventbus"] = detect_eventbus_issues(); 
    report["config_async"] = detect_config_async_issues();
    report["tasks"] = detect_task_issues();
    
    // Growth analysis if we have baseline
    if (baseline_snapshot.timestamp_us > 0) {
        report["growth_analysis"] = analyze_resource_growth(baseline_snapshot, current);
    }
    
    // Overall health assessment
    bool has_memory_issues = report["memory_leaks"]["indicators"]["potential_leak"];
    bool has_eventbus_issues = report["eventbus"]["issues"]["has_issues"];
    bool has_config_issues = report["config_async"]["issues"]["has_issues"];
    bool has_task_issues = report["tasks"]["issues"]["has_issues"];
    
    report["overall"]["has_issues"] = has_memory_issues || has_eventbus_issues || has_config_issues || has_task_issues;
    report["overall"]["severity"] = "unknown";
    
    if (current.cpu_usage_percent > 90 || current.free_heap_bytes < 10240) {
        report["overall"]["severity"] = "critical";
    } else if (current.cpu_usage_percent > 70 || has_memory_issues || has_eventbus_issues) {
        report["overall"]["severity"] = "warning";
    } else {
        report["overall"]["severity"] = "normal";
    }
    
    return report;
}

esp_err_t start_continuous_monitoring(uint32_t check_interval_ms) {
    if (monitoring_active) {
        ESP_LOGW(TAG, "Monitoring already active");
        return ESP_OK;
    }
    
    monitor_interval_ms = check_interval_ms;
    
    BaseType_t ret = xTaskCreate(
        monitor_task,
        "diagnostic_monitor",
        4096,
        nullptr,
        2, // Low priority
        &monitor_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create monitoring task");
        return ESP_FAIL;
    }
    
    monitoring_active = true;
    ESP_LOGI(TAG, "Started continuous monitoring (interval: %lu ms)", check_interval_ms);
    
    return ESP_OK;
}

void stop_continuous_monitoring() {
    if (!monitoring_active) {
        return;
    }
    
    monitoring_active = false;
    
    if (monitor_task_handle) {
        vTaskDelete(monitor_task_handle);
        monitor_task_handle = nullptr;
    }
    
    ESP_LOGI(TAG, "Stopped continuous monitoring");
}

size_t force_system_cleanup() {
    ESP_LOGI(TAG, "Forcing system cleanup");
    
    size_t initial_heap = esp_get_free_heap_size();
    
    // Clear EventBus non-critical events
    EventBus::clear();
    
    // Force config flush if pending
#ifdef CONFIG_USE_ASYNC_SAVE
    ConfigManagerAsync::flush_pending_saves(1000);
#endif
    
    // Force heap integrity check
    esp_err_t heap_ok = heap_caps_check_integrity_all(true);
    if (heap_ok != ESP_OK) {
        ESP_LOGW(TAG, "Heap integrity check failed: %s", esp_err_to_name(heap_ok));
    }
    
    size_t final_heap = esp_get_free_heap_size();
    size_t freed = final_heap > initial_heap ? final_heap - initial_heap : 0;
    
    ESP_LOGI(TAG, "System cleanup completed, freed %zu bytes", freed);
    
    return freed;
}

// Private helper functions

static void monitor_task(void* param) {
    ESP_LOGI(TAG, "Diagnostic monitor task started");
    
    TickType_t last_wake = xTaskGetTickCount();
    
    while (monitoring_active) {
        ResourceSnapshot snapshot = take_snapshot();
        
        // Add to history
        history.push_back(snapshot);
        if (history.size() > 100) {
            history.erase(history.begin());
        }
        
        // Check for immediate issues
        bool needs_alert = false;
        std::string alert_reason;
        
        if (snapshot.cpu_usage_percent > 90) {
            needs_alert = true;
            alert_reason += "High CPU ";
        }
        
        if (snapshot.free_heap_bytes < 15360) { // <15KB
            needs_alert = true;
            alert_reason += "Low memory ";
        }
        
        if (snapshot.eventbus_queue_size > 20) {
            needs_alert = true;
            alert_reason += "EventBus backlog ";
        }
        
        if (snapshot.config_pending_async_saves > 5) {
            needs_alert = true;
            alert_reason += "Config backlog ";
        }
        
        if (needs_alert) {
            ESP_LOGW(TAG, "DIAGNOSTIC ALERT: %s(CPU:%u%%, Heap:%zuKB, EB queue:%zu, Config pending:%zu)",
                     alert_reason.c_str(),
                     snapshot.cpu_usage_percent,
                     snapshot.free_heap_bytes / 1024,
                     snapshot.eventbus_queue_size,
                     snapshot.config_pending_async_saves);
        } else {
            ESP_LOGI(TAG, "System OK: CPU:%u%%, Heap:%zuKB, EB:%zu, Cfg:%zu",
                     snapshot.cpu_usage_percent,
                     snapshot.free_heap_bytes / 1024,
                     snapshot.eventbus_queue_size,
                     snapshot.config_pending_async_saves);
        }
        
        // Compare with baseline every 10 samples
        static int sample_count = 0;
        if (++sample_count >= 10) {
            auto growth_analysis = analyze_resource_growth(baseline_snapshot, snapshot);
            
            if (growth_analysis["risk_assessment"]["overall_risk"]) {
                ESP_LOGW(TAG, "RESOURCE GROWTH DETECTED: CPU delta: %d%%, Heap delta: %d KB",
                         (int)growth_analysis["cpu"]["usage_delta_percent"],
                         (int)growth_analysis["memory"]["heap_delta_bytes"] / 1024);
            }
            
            sample_count = 0;
        }
        
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(monitor_interval_ms));
    }
    
    ESP_LOGI(TAG, "Diagnostic monitor task ended");
    vTaskDelete(nullptr);
}

static size_t estimate_json_memory_usage() {
    // Rough estimation of JSON memory usage
    size_t estimated = 0;
    
    // Safe config size estimation without exceptions
    auto config_json = ConfigManager::get("");
    if (!config_json.empty() && !config_json.is_null()) {
        // Only process if config is valid JSON
        if (config_json.is_object() || config_json.is_array()) {
            std::string serialized = config_json.dump();
            estimated += serialized.length() * 2; // Rough estimation including JSON overhead
        }
    }
    
    return estimated;
}

static size_t count_freertos_tasks() {
    return uxTaskGetNumberOfTasks();
}

} // namespace DiagnosticTools 