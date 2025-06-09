#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config_manager.h"
#include "module_manager.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <freertos/task.h>

static const char* TAG = "Application";

namespace Application {

// Internal state
static State current_state = State::BOOT;
static uint32_t boot_time_ms = 0;
static uint32_t cycle_count = 0;
static uint32_t error_count = 0;
static uint32_t last_health_check_ms = 0;

// Performance metrics
static uint32_t min_cycle_time_us = UINT32_MAX;
static uint32_t max_cycle_time_us = 0;
static uint32_t total_cycle_time_us = 0;

// Configuration
static constexpr uint32_t MAIN_LOOP_PERIOD_MS = 10;     // 100Hz
static constexpr uint32_t HEALTH_CHECK_PERIOD_MS = 1000; // 1Hz
static constexpr uint32_t MODULE_UPDATE_BUDGET_MS = 8;   // 8ms for modules
static constexpr uint32_t EVENT_PROCESS_BUDGET_MS = 2;   // 2ms for events

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
    
    // Transition to INIT state
    current_state = State::INIT;
    ESP_LOGI(TAG, "Transitioning to INIT state");
    
    // Load configuration
    ret = ConfigManager::load();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config, using defaults");
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
    
    // Transition to RUNNING
    current_state = State::RUNNING;
    ESP_LOGI(TAG, "System initialization complete");
    
    // Publish startup event
    EventBus::publish("system.started", {
        {"uptime_ms", get_uptime_ms()},
        {"free_heap", get_free_heap()}
    });
    
    return ESP_OK;
}

[[noreturn]] void run() {
    ESP_LOGI(TAG, "Main loop starting @ %dHz", 1000 / MAIN_LOOP_PERIOD_MS);
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (1) {
        if (current_state != State::RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        
        uint32_t cycle_start = esp_timer_get_time();
        
        // 1. Update all modules (8ms budget)
        ModuleManager::tick_all(MODULE_UPDATE_BUDGET_MS);
        
        // 2. Process events (2ms budget)
        EventBus::process(EVENT_PROCESS_BUDGET_MS);
        
        // 3. Health check (1Hz)
        uint32_t now_ms = esp_timer_get_time() / 1000;
        if (now_ms - last_health_check_ms >= HEALTH_CHECK_PERIOD_MS) {
            check_health();
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
    if (free_heap < 10240) { // < 10KB
        ESP_LOGW(TAG, "Low heap: %u bytes", free_heap);
        healthy = false;
    }
    
    // Check CPU usage
    uint8_t cpu = get_cpu_usage();
    if (cpu > 90) {
        ESP_LOGW(TAG, "High CPU usage: %d%%", cpu);
        healthy = false;
    }
    
    // Check modules
    auto health_report = ModuleManager::get_health_report();
    if (health_report.error_modules > 0 || health_report.degraded_modules > 0) {
        healthy = false;
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
    if (cycle_count == 0) return 0;
    
    uint32_t avg_cycle_us = total_cycle_time_us / cycle_count;
    return (avg_cycle_us * 100) / (MAIN_LOOP_PERIOD_MS * 1000);
}

size_t get_stack_high_water_mark() {
    return uxTaskGetStackHighWaterMark(NULL);
}

} // namespace Application