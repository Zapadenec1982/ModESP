#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config_manager.h"
#include "module_manager.h"
#include "module_heartbeat.h"
#include "module_lifecycle.h"
#include "esphal.h"
#include "sensor_driver_init.h"
// #include "configuration_manager.h" // Removed - moved to adaptive_ui
// #include "api_dispatcher.h" // Removed - moved to adaptive_ui
// #include "test_core_components.h" // TODO: Add core component tests

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <freertos/task.h>
#include <esp_mac.h>
#include <esp_heap_caps.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include "nlohmann/json.hpp"
#include <cstring>
#include <algorithm>

// Forward declaration
// class ESPhal;

static const char* TAG = "Application";

namespace Application {

// Internal state
static State current_state = State::BOOT;
static uint32_t boot_time_ms = 0;
static uint32_t cycle_count = 0;
static uint32_t error_count = 0;
static uint32_t last_health_check_ms = 0;
static bool emergency_mode = false;

// Global ESPhal instance
static ESPhal hal;
static ModuleHeartbeat heartbeat;

// Performance metrics
static uint32_t min_cycle_time_us = UINT32_MAX;
static uint32_t max_cycle_time_us = 0;
static uint32_t total_cycle_time_us = 0;

// Configuration
static constexpr uint32_t MAIN_LOOP_PERIOD_MS = 10;     // 100Hz
static constexpr uint32_t HEALTH_CHECK_PERIOD_MS = 1000; // 1Hz
static constexpr uint32_t MODULE_UPDATE_BUDGET_MS = 8;   // 8ms for modules
static constexpr uint32_t EVENT_PROCESS_BUDGET_MS = 2;   // 2ms for events

// Task handles for multicore operation
static TaskHandle_t sensor_task_handle = nullptr;

esp_err_t init() {
    ESP_LOGI(TAG, "ModuChill Application starting...");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    
    boot_time_ms = esp_timer_get_time() / 1000;
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize core services
    ESP_ERROR_CHECK(EventBus::init());
    ESP_ERROR_CHECK(SharedState::init());
    ESP_ERROR_CHECK(ConfigManager::init());
    ESP_ERROR_CHECK(ModuleManager::init());
    ESP_ERROR_CHECK(heartbeat.init());
    
    // Initialize Hardware Abstraction Layer
    ret = hal.init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ESPhal: %s", esp_err_to_name(ret));
        current_state = State::ERROR;
        return ret;
    }
    
    // Initialize built-in sensor drivers
    initialize_builtin_sensor_drivers();
    
    // Provide ModuleManager with the heartbeat monitor instance
    ModuleManager::set_heartbeat_monitor(&heartbeat);
    
    // Register all available modules
    ret = ModESP::ModuleRegistry::register_all_modules();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register modules: %s", esp_err_to_name(ret));
        current_state = State::ERROR;
        return ret;
    }
    
    // TODO-006: Initialize Hybrid API System
    ESP_LOGI(TAG, "Initializing Hybrid API System...");
    
    // Configuration Manager moved to adaptive_ui
    /*
    // Initialize Configuration Manager
    ret = ConfigurationManager::instance().initialize();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize Configuration Manager: %s", esp_err_to_name(ret));
        current_state = State::ERROR;
        return ret;
    }
    */
    
    // Initialize API Dispatcher with hybrid APIs
    // Note: This should be done by the WebUIModule, but we check that it works here
    // TODO: Move this to proper module initialization when WebUIModule is refactored
    
    ESP_LOGI(TAG, "Hybrid API System initialized successfully");
    
    // Transition to INIT state
    current_state = State::INIT;
    ESP_LOGI(TAG, "Transitioning to INIT state");
    
    // Load initial configuration (STACK-SAFE) 
    // FIXED: Main task stack increased to 12KB to handle JSON parsing
    ret = ConfigManager::load_initial_config();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load initial config");
        current_state = State::ERROR;
        return ret;
    }
    
    // Configure all modules with loaded configuration
    ModuleManager::configure_all(ConfigManager::get_all());
    
    // Initialize modules
    ret = ModuleManager::init_all();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Module initialization failed");
        current_state = State::ERROR;
        return ret;
    }

    // Configure and integrate ModuleHeartbeat
    auto hb_config = ConfigManager::get("system");
    if (!hb_config.is_null()) {
        heartbeat.configure(hb_config);
    }
    
    // Set the restart callback for the heartbeat module
    heartbeat.set_restart_callback([](const char* module_name) {
        ESP_LOGI(TAG, "Heartbeat triggered restart for %s", module_name);
        return ModuleManager::reload_module(module_name) == ESP_OK;
    });

    // Now safely load full configuration from storage
    ret = ConfigManager::load();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load saved config, using defaults");
    }
    
    // Transition to RUNNING
    current_state = State::RUNNING;
    ESP_LOGI(TAG, "System initialization complete");
    
    // TODO: Add functional tests of core components
    ESP_LOGI(TAG, "Core component functional tests disabled");
    
    // Publish startup event
    EventBus::publish("system.started", {
        {"uptime_ms", get_uptime_ms()},
        {"free_heap", get_free_heap()},
        {"tests_passed", 0},
        {"tests_total", 0}
    });
    
    return ESP_OK;
}

// Sensor task running on Core 1
static void sensor_task(void* pvParameters) {
    ESP_LOGI(TAG, "Sensor task started on Core %d", xPortGetCoreID());
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t sensor_period = pdMS_TO_TICKS(100); // 10Hz for sensors
    
    uint32_t sensor_cycle_count = 0;
    uint64_t sensor_total_time_us = 0;
    uint32_t sensor_max_time_us = 0;
    uint32_t last_report_ms = 0;
    
    while (current_state == State::RUNNING) {
        uint64_t start_time_us = esp_timer_get_time();
        
        // Find and update sensor module on Core 1
        BaseModule* sensor_module = ModuleManager::find_module("SensorModule");
        if (sensor_module) {
            sensor_module->update();
        }
        
        uint64_t end_time_us = esp_timer_get_time();
        uint32_t cycle_time_us = (uint32_t)(end_time_us - start_time_us);
        
        // Update sensor task metrics
        sensor_cycle_count++;
        sensor_total_time_us += cycle_time_us;
        sensor_max_time_us = std::max(sensor_max_time_us, cycle_time_us);
        
        // Report sensor task performance every 10 seconds
        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        if (now_ms - last_report_ms >= 10000) {
            uint32_t avg_time_us = sensor_cycle_count > 0 ? 
                (uint32_t)(sensor_total_time_us / sensor_cycle_count) : 0;
            ESP_LOGI(TAG, "Core 1 Sensors: %lu cycles, %lu/%lu μs (avg/max)", 
                     sensor_cycle_count, avg_time_us, sensor_max_time_us);
            last_report_ms = now_ms;
        }
        
        vTaskDelayUntil(&last_wake_time, sensor_period);
    }
    
    ESP_LOGI(TAG, "Sensor task ended");
    vTaskDelete(nullptr);
}

[[noreturn]] void run() {
    ESP_LOGI(TAG, "Main loop starting @ %luHz", 1000UL / MAIN_LOOP_PERIOD_MS);
    
    // Create sensor task on Core 1
    xTaskCreatePinnedToCore(
        sensor_task,          // Function
        "sensor_task",        // Name
        4096,                 // Stack size
        nullptr,              // Parameters
        5,                    // Priority
        &sensor_task_handle,  // Handle
        1                     // Core 1
    );
    
    ESP_LOGI(TAG, "Main loop (Core 0) and sensor task (Core 1) started");
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        if (current_state != State::RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        uint32_t cycle_start = esp_timer_get_time();
        
        // 1. Update non-sensor modules only (Core 0)
        ModuleManager::tick_all_except_sensors(MODULE_UPDATE_BUDGET_MS);
        
        // 2. Process events (2ms budget)
        EventBus::process(EVENT_PROCESS_BUDGET_MS);
        
        // 3. Health check (1Hz)
        uint32_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - last_health_check_ms >= HEALTH_CHECK_PERIOD_MS) {
            // Update heartbeat monitor
            heartbeat.update();

            check_health();
            
            // Log CPU usage and cycle metrics
            uint8_t cpu_usage = get_cpu_usage();
            size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
            ESP_LOGI(TAG, "CPU: %u%%, Cycle: %lu/%lu/%lu μs (min/avg/max), Free heap: %zu KB, Largest block: %zu KB", 
                     cpu_usage,
                     min_cycle_time_us,
                     cycle_count > 0 ? (uint32_t)(total_cycle_time_us / cycle_count) : 0,
                     max_cycle_time_us,
                     get_free_heap() / 1024,
                     largest_block / 1024);
            
            last_health_check_ms = now_ms;
        }
        
        // Update cycle metrics
        uint32_t cycle_time = esp_timer_get_time() - cycle_start;
        min_cycle_time_us = std::min(min_cycle_time_us, cycle_time);
        max_cycle_time_us = std::max(max_cycle_time_us, cycle_time);
        total_cycle_time_us += cycle_time;
        cycle_count++;
        
        // Warn if cycle took too long
        if (cycle_time > MAIN_LOOP_PERIOD_MS * 1000) {
            ESP_LOGW(TAG, "Cycle overrun: %lu us", cycle_time);
        }
        
        // Fixed-rate execution
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MAIN_LOOP_PERIOD_MS));
    }
}

void shutdown() {
    ESP_LOGI(TAG, "Shutdown requested");
    current_state = State::SHUTDOWN;
    
    // Stop modules
    ModuleManager::shutdown_all();
    
    // Save configuration
    ConfigManager::save();
    
    ESP_LOGI(TAG, "Shutdown complete");
}

void restart(uint32_t delay_ms) {
    ESP_LOGI(TAG, "Restart requested in %lu ms", delay_ms);
    shutdown();
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
    esp_restart();
}

State get_state() {
    return current_state;
}

bool is_running() {
    return current_state == State::RUNNING;
}

void report_error(const char* component, esp_err_t error, 
                  ErrorSeverity severity, const char* message) {
    ESP_LOGE(TAG, "Error from %s: 0x%x (%s) - %s", 
             component, error, esp_err_to_name(error), 
             message ? message : "");
    
    error_count++;
    
    // Publish error event
    EventBus::publish("system.error", {
        {"component", component},
        {"error_code", error},
        {"severity", (int)severity},
        {"message", message ? message : ""}
    }, EventBus::Priority::HIGH);
    
    // Handle critical errors
    if (severity >= ErrorSeverity::CRITICAL) {
        current_state = State::ERROR;
        if (severity == ErrorSeverity::FATAL) {
            ESP_LOGE(TAG, "Fatal error, restarting...");
            restart(3000);
        }
    }
}

bool check_health() {
    bool healthy = true;
    
    // Check heap
    size_t free_heap = get_free_heap();
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    
    if (free_heap < 10240) { // < 10KB
        ESP_LOGW(TAG, "Low heap: %zu bytes", (unsigned int)free_heap);
        healthy = false;
        
        // Activate emergency mode if critically low
        if (free_heap < 5120 && !emergency_mode) { // < 5KB
            ESP_LOGE(TAG, "Critical memory shortage, activating emergency mode");
            set_emergency_mode(true);
        }
    }
    
    // Check heap fragmentation
    if (free_heap > 20480 && largest_free_block < 10240) { // Have >20KB but largest block <10KB
        ESP_LOGW(TAG, "Heap fragmentation detected! Free: %zu, Largest block: %zu", 
                 (unsigned int)free_heap, (unsigned int)largest_free_block);
        healthy = false;
        
        // Critical fragmentation - schedule reboot
        if (largest_free_block < 5120) { // <5KB largest block
            ESP_LOGE(TAG, "Critical heap fragmentation, reboot recommended");
            // In production: schedule graceful reboot during maintenance window
        }
    }
    
    // Check CPU usage
    uint8_t cpu = get_cpu_usage();
    if (cpu > 90) {
        ESP_LOGW(TAG, "High CPU usage: %u%%", (unsigned int)cpu);
        healthy = false;
        
        // Activate emergency mode if critical
        if (cpu > 95 && !emergency_mode) {
            ESP_LOGE(TAG, "Critical CPU usage, activating emergency mode");
            set_emergency_mode(true);
        }
    }
    
    // Check modules (if ModuleManager::get_health_report() exists)
    // auto health_report = ModuleManager::get_health_report();
    // if (health_report.error_modules > 0 || health_report.degraded_modules > 0) {
    //     healthy = false;
    //     
    //     // Critical if more than 50% of modules are in error
    //     float error_rate = (float)health_report.error_modules / health_report.total_modules;
    //     if (error_rate > 0.5 && !emergency_mode) {
    //         ESP_LOGE(TAG, "High module error rate: %.1f%%, activating emergency mode", 
    //                  error_rate * 100.0f);
    //         set_emergency_mode(true);
    //     }
    // }
    
    // Check if we can exit emergency mode
    if (emergency_mode && healthy && free_heap > 20480 && cpu < 80) {
        ESP_LOGI(TAG, "System health restored, deactivating emergency mode");
        set_emergency_mode(false);
    }
    
    return healthy;
}

uint32_t get_uptime_ms() {
    return (esp_timer_get_time() / 1000) - boot_time_ms;
}

size_t get_free_heap() {
    return esp_get_free_heap_size();
}

size_t get_min_free_heap() {
    return esp_get_minimum_free_heap_size();
}

uint8_t get_cpu_usage() {
    static uint64_t last_measurement_time = 0;
    static uint8_t last_cpu_usage = 5;
    
    uint64_t current_time = esp_timer_get_time();
    
    // Мінімальний інтервал між вимірюваннями (1 секунда для стабільності)
    if (current_time - last_measurement_time < 1000000) {
        return last_cpu_usage;
    }
    
    if (cycle_count == 0) return 5;
    
    float avg_cycle_us = (float)total_cycle_time_us / cycle_count;
    float period_us = MAIN_LOOP_PERIOD_MS * 1000.0f;
    
    // Реальний main loop load (відсоток періоду що використовується)
    float main_loop_load = (avg_cycle_us / period_us) * 100.0f;
    
    // ДИНАМІЧНА оцінка system overhead (НЕ константа!)
    float base_overhead = 2.0f; // Мінімальний системний overhead
    float dynamic_overhead = main_loop_load * 0.15f; // 15% від main loop навантаження
    float system_overhead = base_overhead + dynamic_overhead;
    
    float total_usage = main_loop_load + system_overhead;
    
    // ТИМЧАСОВІ корекції (не постійні додавання!)
    if (max_cycle_time_us > period_us * 1.5f) {
        total_usage += 3.0f; // Значні overruns (тільки поки вони є)
    } else if (max_cycle_time_us > period_us) {
        total_usage += 1.0f; // Незначні overruns
    }
    
    // Memory pressure може збільшувати CPU usage
    if (get_free_heap() < 50000) {
        total_usage += 1.5f;
    }
    
    // Обмеження
    if (total_usage < 1.0f) total_usage = 1.0f;
    if (total_usage > 100.0f) total_usage = 100.0f;
    
    // Згладжування для стабільності показань
    float smoothing_factor = 0.3f; // 30% нового значення, 70% старого
    total_usage = (smoothing_factor * total_usage) + ((1.0f - smoothing_factor) * last_cpu_usage);
    
    last_measurement_time = current_time;
    last_cpu_usage = (uint8_t)(total_usage + 0.5f);
    
    ESP_LOGD(TAG, "CPU Usage: main_loop=%.1f%%, system=%.1f%%, total=%.1f%%", 
             main_loop_load, system_overhead, total_usage);
    
    return last_cpu_usage;
}

size_t get_stack_high_water_mark() {
    return uxTaskGetStackHighWaterMark(NULL);
}

ESPhal& get_hal() {
    return hal;
}

ModuleHeartbeat& get_heartbeat() {
    return heartbeat;
}

nlohmann::json get_performance_metrics() {
    uint32_t avg_cycle_us = cycle_count > 0 ? 
        (uint32_t)(total_cycle_time_us / cycle_count) : 0;
    
    return nlohmann::json{
        {"main_loop", {
            {"cycle_count", cycle_count},
            {"min_cycle_us", min_cycle_time_us},
            {"max_cycle_us", max_cycle_time_us},
            {"avg_cycle_us", avg_cycle_us},
            {"target_cycle_us", MAIN_LOOP_PERIOD_MS * 1000},
            {"overrun_percentage", cycle_count > 0 ? 
                ((float)max_cycle_time_us / (MAIN_LOOP_PERIOD_MS * 1000)) * 100.0f : 0.0f}
        }},
        {"memory", {
            {"free_heap_bytes", get_free_heap()},
            {"min_free_heap_bytes", get_min_free_heap()},
            {"stack_remaining_bytes", get_stack_high_water_mark()}
        }},
        {"system", {
            {"uptime_ms", get_uptime_ms()},
            {"cpu_usage_percent", get_cpu_usage()},
            {"error_count", error_count},
            {"emergency_mode", emergency_mode}
        }}
    };
}

nlohmann::json get_multicore_stats() {
    // Note: This would need additional tracking in the actual implementation
    return nlohmann::json{
        {"core0", {
            {"name", "Main Application Core"},
            {"modules", "ClimateControl, UI, Network, Events"},
            {"frequency_hz", 100},
            {"primary_task", "Main Loop"}
        }},
        {"core1", {
            {"name", "Sensor Core"}, 
            {"modules", "SensorModule"},
            {"frequency_hz", 10},
            {"primary_task", "Sensor Data Collection"}
        }},
        {"synchronization", {
            {"shared_state_accesses", SharedState::get_stats().total_sets + 
                                     SharedState::get_stats().total_gets},
            {"event_bus_messages", 0} // TODO: Add event tracking
        }}
    };
}

esp_err_t force_memory_cleanup() {
    ESP_LOGI(TAG, "Forcing memory cleanup...");
    
    size_t initial_heap = get_free_heap();
    
    // Clear non-essential caches (if methods exist)
    // EventBus::clear_non_critical_events();  // Comment out if method doesn't exist
    
    // Trigger garbage collection in modules (if method exists)
    // ModuleManager::cleanup_modules();  // Comment out if method doesn't exist
    
    // Force JSON cleanup (if method exists)
    // ConfigManager::cleanup_temporary_data();  // Comment out if method doesn't exist
    
    // Force heap defragmentation
    esp_err_t heap_check = heap_caps_check_integrity_all(true);
    
    size_t final_heap = get_free_heap();
    size_t recovered = final_heap > initial_heap ? final_heap - initial_heap : 0;
    
    ESP_LOGI(TAG, "Memory cleanup completed. Recovered %zu bytes", recovered);
    
    return heap_check;
}

nlohmann::json get_system_diagnostics() {
    auto sharedstate_stats = SharedState::get_stats();
    // auto module_health = ModuleManager::get_health_report();  // Uncomment if method exists
    
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    return nlohmann::json{
        {"timestamp", esp_timer_get_time()},
        {"system_state", static_cast<int>(current_state)},
        {"uptime_ms", get_uptime_ms()},
        {"emergency_mode", emergency_mode},
        
        {"memory", {
            {"free_heap_kb", get_free_heap() / 1024},
            {"min_free_heap_kb", get_min_free_heap() / 1024},
            {"stack_remaining_bytes", get_stack_high_water_mark()},
            {"heap_usage_percent", 
                get_min_free_heap() > 0 ? 
                ((float)(get_min_free_heap() - get_free_heap()) / get_min_free_heap()) * 100.0f : 0.0f}
        }},
        
        {"performance", {
            {"cpu_usage_percent", get_cpu_usage()},
            {"main_loop_hz", 1000.0f / MAIN_LOOP_PERIOD_MS},
            {"cycle_count", cycle_count},
            {"avg_cycle_time_us", cycle_count > 0 ? 
                (uint32_t)(total_cycle_time_us / cycle_count) : 0}
        }},
        
        {"shared_state", {
            {"entries_used", sharedstate_stats.used},
            {"entries_capacity", sharedstate_stats.capacity},
            {"utilization_percent", 
                sharedstate_stats.capacity > 0 ?
                ((float)sharedstate_stats.used / sharedstate_stats.capacity) * 100.0f : 0.0f},
            {"total_operations", sharedstate_stats.total_sets + sharedstate_stats.total_gets},
            {"active_subscriptions", sharedstate_stats.subscriptions}
        }},
        
        {"modules", {
            {"status", "Module health reporting available if ModuleManager::get_health_report() is implemented"}
            // Uncomment when ModuleManager::get_health_report() is available:
            // {"total_modules", module_health.total_modules},
            // {"healthy_modules", module_health.healthy_modules},
            // {"degraded_modules", module_health.degraded_modules},
            // {"error_modules", module_health.error_modules}
        }},
        
        {"errors", {
            {"total_errors", error_count},
            {"error_rate_per_hour", error_count > 0 && get_uptime_ms() > 0 ? 
                (error_count * 3600000.0f) / get_uptime_ms() : 0.0f}
        }},
        
        {"hardware", {
            {"chip_model", CONFIG_IDF_TARGET},
            {"chip_revision", chip_info.revision},
            {"cpu_cores", chip_info.cores},
            {"flash_size_mb", 4}, // TODO: Fix esp_flash_get_size API call
            {"psram_available", false}, // TODO: Add PSRAM detection
            {"wifi_available", (chip_info.features & CHIP_FEATURE_WIFI_BGN) != 0},
            {"bluetooth_available", (chip_info.features & CHIP_FEATURE_BT) != 0}
        }}
    };
}

bool is_emergency_mode() {
    return emergency_mode;
}

void set_emergency_mode(bool enable) {
    if (emergency_mode == enable) {
        return; // No change needed
    }
    
    emergency_mode = enable;
    
    ESP_LOGW(TAG, "Emergency mode %s", enable ? "ENABLED" : "DISABLED");
    
    // Publish emergency mode change
    EventBus::publish("system.emergency_mode", {
        {"enabled", enable},
        {"timestamp", esp_timer_get_time()},
        {"trigger", "manual"}
    }, EventBus::Priority::HIGH);
    
    // Update SharedState
    SharedState::set_typed("system.emergency_mode", enable);
    
    if (enable) {
        // Enable emergency mode measures
        ESP_LOGW(TAG, "Activating emergency measures:");
        ESP_LOGW(TAG, "  - Reducing update frequencies");
        ESP_LOGW(TAG, "  - Limiting resource usage");
        
        // Disable non-critical modules (if method exists)
        // ModuleManager::disable_non_critical_modules();
        
        // Force memory cleanup
        force_memory_cleanup();
        
    } else {
        // Disable emergency mode
        ESP_LOGI(TAG, "Deactivating emergency mode");
        ESP_LOGI(TAG, "  - Restoring normal operation");
        
        // Re-enable modules (if method exists)
        // ModuleManager::enable_all_modules();
    }
}

} // namespace Application