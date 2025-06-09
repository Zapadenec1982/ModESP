#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config.h"
#include "module_manager.h"
#include "ui_manager.h"

#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ModuChill {

static const char* TAG = "Application";

// Singleton instance
Application* Application::instance_ = nullptr;

// Configuration constants
constexpr uint32_t MAIN_LOOP_PERIOD_MS = 10;    // 100Hz
constexpr uint32_t HEALTH_CHECK_PERIOD_MS = 1000; // 1Hz
constexpr uint32_t WATCHDOG_TIMEOUT_MS = 5000;   // 5 seconds
constexpr uint32_t MODULE_UPDATE_BUDGET_MS = 8;   // 8ms for modules
constexpr uint32_t EVENT_PROCESS_BUDGET_MS = 2;   // 2ms for events

Application::Application() 
    : state_(SystemState::BOOT)
    , boot_time_ms_(0)
    , cycle_count_(0)
    , error_count_(0)
    , last_health_check_ms_(0)
    , min_cycle_time_us_(UINT32_MAX)
    , max_cycle_time_us_(0)
    , total_cycle_time_us_(0)
    , watchdog_timer_(nullptr) {
}

Application* Application::get_instance() {
    if (!instance_) {
        instance_ = new Application();
    }
    return instance_;
}

void Application::init() {
    auto* app = get_instance();
    
    ESP_LOGI(TAG, "ModuChill Application starting...");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    
    app->boot_time_ms_ = esp_timer_get_time() / 1000;
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize core services
    EventBus::init();
    SharedState::init();
    Config::init();
    ModuleManager::init();
    UIManager::init();
    
    // Create watchdog timer
    const esp_timer_create_args_t watchdog_args = {
        .callback = &watchdog_callback,
        .arg = app,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "watchdog"
    };
    ESP_ERROR_CHECK(esp_timer_create(&watchdog_args, &app->watchdog_timer_));
    
    // Transition to INIT state
    app->transition_to_init();
}

esp_err_t Application::transition_to_init() {
    ESP_LOGI(TAG, "Transitioning to INIT state");
    state_ = SystemState::INIT;
    
    // Load configuration
    esp_err_t ret = Config::load();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load config, using defaults");
    }
    
    // Configure modules
    auto config = Config::get_all();
    ModuleManager::configure_modules(config);
    
    // Initialize modules
    ret = ModuleManager::init_modules();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Module initialization failed");
        transition_to_error(ErrorSeverity::CRITICAL);
        return ret;
    }
    
    // Start watchdog
    ESP_ERROR_CHECK(esp_timer_start_periodic(watchdog_timer_, WATCHDOG_TIMEOUT_MS * 1000));
    
    return transition_to_running();
}

esp_err_t Application::transition_to_running() {
    ESP_LOGI(TAG, "Transitioning to RUNNING state");
    state_ = SystemState::RUNNING;
    
    // Publish state change event
    EventBus::publish("system.state.changed", {
        {"old_state", "INIT"},
        {"new_state", "RUNNING"},
        {"uptime_ms", get_uptime_ms()}
    });
    
    return ESP_OK;
}

void Application::run() {
    auto* app = get_instance();
    
    ESP_LOGI(TAG, "Main loop starting @ %dHz", 1000 / MAIN_LOOP_PERIOD_MS);
    
    TickType_t last_wake_time = xTaskGetTickCount();
    
    while (app->state_ == SystemState::RUNNING) {
        app->main_loop_cycle();
        
        // Fixed-rate execution
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MAIN_LOOP_PERIOD_MS));
    }
    
    ESP_LOGI(TAG, "Main loop exited, state: %d", (int)app->state_);
}

void Application::main_loop_cycle() {
    uint32_t cycle_start = esp_timer_get_time();
    
    // 1. Update all modules (8ms budget)
    ModuleManager::tick_all(MODULE_UPDATE_BUDGET_MS);
    
    // 2. Process events (2ms budget)
    EventBus::process(EVENT_PROCESS_BUDGET_MS);
    
    // 3. Health check (1Hz)
    uint32_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - last_health_check_ms_ >= HEALTH_CHECK_PERIOD_MS) {
        check_system_health();
        last_health_check_ms_ = now_ms;
    }
    
    // 4. Feed watchdog
    feed_watchdog();
    
    // Update cycle metrics
    uint32_t cycle_time = esp_timer_get_time() - cycle_start;
    min_cycle_time_us_ = std::min(min_cycle_time_us_, cycle_time);
    max_cycle_time_us_ = std::max(max_cycle_time_us_, cycle_time);
    total_cycle_time_us_ += cycle_time;
    cycle_count_++;
    
    // Warn if cycle took too long
    if (cycle_time > MAIN_LOOP_PERIOD_MS * 1000) {
        ESP_LOGW(TAG, "Cycle overrun: %lu us", cycle_time);
    }
}

void Application::check_system_health() {
    auto health = get_system_health();
    
    // Check heap
    if (health.free_heap < 10240) { // < 10KB
        ESP_LOGW(TAG, "Low heap: %lu bytes", health.free_heap);
        report_error("system", ESP_ERR_NO_MEM, ErrorSeverity::MAJOR, "Low heap memory");
    }
    
    // Check CPU usage
    if (health.cpu_usage > 90) {
        ESP_LOGW(TAG, "High CPU usage: %d%%", health.cpu_usage);
    }
    
    // Module health check
    ModuleManager::check_health();
}

void Application::feed_watchdog() {
    // In a real implementation, this would pet a hardware watchdog
    // For now, just reset the software timer
    esp_timer_stop(watchdog_timer_);
    esp_timer_start_periodic(watchdog_timer_, WATCHDOG_TIMEOUT_MS * 1000);
}

void Application::watchdog_callback(void* arg) {
    ESP_LOGE(TAG, "Watchdog timeout! System appears frozen");
    // In production, this would trigger a hardware reset
    esp_restart();
}

SystemHealth Application::get_system_health() {
    auto* app = get_instance();
    
    SystemHealth health;
    health.state = app->state_;
    health.uptime_ms = get_uptime_ms();
    health.free_heap = esp_get_free_heap_size();
    health.error_count = app->error_count_;
    health.restart_count = 0; // Would be loaded from NVS
    
    // Calculate CPU usage (simplified)
    if (app->cycle_count_ > 0) {
        uint32_t avg_cycle_us = app->total_cycle_time_us_ / app->cycle_count_;
        health.cpu_usage = (avg_cycle_us * 100) / (MAIN_LOOP_PERIOD_MS * 1000);
    } else {
        health.cpu_usage = 0;
    }
    
    // Overall health score
    health.overall_score = 100;
    if (health.free_heap < 20480) health.overall_score -= 20;
    if (health.cpu_usage > 80) health.overall_score -= 20;
    if (app->error_count_ > 0) health.overall_score -= std::min(30u, app->error_count_ * 5);
    
    return health;
}

void Application::report_error(const char* component, esp_err_t error, 
                             ErrorSeverity severity, const char* message) {
    auto* app = get_instance();
    
    ComponentError err = {
        .component = component,
        .error_code = error,
        .severity = severity,
        .message = message,
        .timestamp = esp_timer_get_time() / 1000
    };
    
    ESP_LOGE(TAG, "Error from %s: 0x%x (%s)", component, error, 
             message ? message : esp_err_to_name(error));
    
    app->error_count_++;
    
    // Publish error event
    EventBus::publish("system.error", {
        {"component", component},
        {"error_code", error},
        {"severity", (int)severity},
        {"message", message ? message : ""},
        {"timestamp", err.timestamp}
    }, EventPriority::HIGH);
    
    // Handle based on severity
    if (severity >= ErrorSeverity::CRITICAL) {
        app->handle_critical_error(err);
    }
}

void Application::handle_critical_error(const ComponentError& error) {
    ESP_LOGE(TAG, "Critical error detected, attempting recovery...");
    
    transition_to_error(error.severity);
    
    if (error.severity == ErrorSeverity::FATAL) {
        ESP_LOGE(TAG, "Fatal error, restarting in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        attempt_recovery();
    }
}

void Application::transition_to_error(ErrorSeverity severity) {
    auto old_state = state_;
    state_ = SystemState::ERROR;
    
    ESP_LOGW(TAG, "System entering ERROR state");
    
    EventBus::publish("system.state.changed", {
        {"old_state", (int)old_state},
        {"new_state", "ERROR"},
        {"severity", (int)severity}
    }, EventPriority::CRITICAL);
}

void Application::attempt_recovery() {
    ESP_LOGI(TAG, "Attempting system recovery...");
    
    // Stop problematic modules
    // ModuleManager::stop_unhealthy_modules();
    
    // Try to recover
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // If recovery successful, transition back to running
    if (ModuleManager::get_healthy_module_count() > 0) {
        ESP_LOGI(TAG, "Recovery successful, resuming operation");
        state_ = SystemState::RUNNING;
    } else {
        ESP_LOGE(TAG, "Recovery failed, system restart required");
        esp_restart();
    }
}

void Application::shutdown() {
    auto* app = get_instance();
    
    ESP_LOGI(TAG, "Shutdown requested");
    app->transition_to_shutdown();
}

void Application::transition_to_shutdown() {
    state_ = SystemState::SHUTDOWN;
    
    ESP_LOGI(TAG, "Stopping modules...");
    ModuleManager::stop_all();
    
    ESP_LOGI(TAG, "Saving configuration...");
    Config::save();
    
    ESP_LOGI(TAG, "Cleanup complete");
    
    // Stop watchdog
    if (watchdog_timer_) {
        esp_timer_stop(watchdog_timer_);
        esp_timer_delete(watchdog_timer_);
    }
}

void Application::restart() {
    shutdown();
    ESP_LOGI(TAG, "Restarting system...");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();
}

// Static getters
SystemState Application::get_state() {
    return get_instance()->state_;
}

bool Application::is_running() {
    return get_instance()->state_ == SystemState::RUNNING;
}

bool Application::is_error() {
    return get_instance()->state_ == SystemState::ERROR;
}

uint32_t Application::get_uptime_ms() {
    return (esp_timer_get_time() / 1000) - get_instance()->boot_time_ms_;
}

uint32_t Application::get_free_heap() {
    return esp_get_free_heap_size();
}

uint8_t Application::get_cpu_usage() {
    return get_system_health().cpu_usage;
}

uint32_t Application::get_cycle_count() {
    return get_instance()->cycle_count_;
}

void Application::check_health() {
    get_instance()->check_system_health();
}

void Application::on_state_change(StateChangeCallback callback) {
    // Would store callbacks and invoke on state changes
    // For now, just use EventBus
}

} // namespace ModuChill