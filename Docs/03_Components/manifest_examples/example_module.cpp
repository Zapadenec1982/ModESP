/**
 * @file example_module.cpp
 * @brief Example module implementation
 */

#include "example_module.h"
#include "module_factory.h"
#include "event_bus.h"
#include "shared_state.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "ExampleModule";

esp_err_t ExampleModule::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing ExampleModule");
    
    // Perform initialization tasks
    // - Set up hardware
    // - Allocate resources
    // - Subscribe to events
    
    // Subscribe to relevant events
    EventBus::subscribe("system.config_changed", [this](const nlohmann::json& data) {
        ESP_LOGI(TAG, "Configuration changed, reloading...");
        // Handle configuration changes
    });
    
    initialized_ = true;
    ESP_LOGI(TAG, "ExampleModule initialized successfully");
    
    return ESP_OK;
}

void ExampleModule::configure(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Configuring ExampleModule");
    
    // Parse configuration safely
    if (config.contains("update_interval_ms") && config["update_interval_ms"].is_number()) {
        update_interval_ms_ = config["update_interval_ms"];
        ESP_LOGI(TAG, "Update interval set to %lu ms", update_interval_ms_);
    }
    
    // Apply other configuration parameters
    if (config.contains("enabled") && config["enabled"].is_boolean()) {
        bool enabled = config["enabled"];
        ESP_LOGI(TAG, "Module %s", enabled ? "enabled" : "disabled");
    }
}

void ExampleModule::update() {
    if (!initialized_) {
        return;
    }
    
    // Check if it's time to update
    uint32_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - last_update_ms_ < update_interval_ms_) {
        return;
    }
    
    last_update_ms_ = now_ms;
    update_count_++;
    
    // Perform module work
    do_work();
    
    // Publish state to SharedState
    nlohmann::json state = {
        {"update_count", update_count_},
        {"last_update_ms", last_update_ms_},
        {"status", "running"}
    };
    SharedState::set("example.state", state);
    
    // Publish event every 10 updates
    if (update_count_ % 10 == 0) {
        EventBus::publish("example.milestone", {
            {"count", update_count_},
            {"timestamp", now_ms}
        });
    }
}

void ExampleModule::stop() {
    ESP_LOGI(TAG, "Stopping ExampleModule");
    
    // Clean up resources
    // - Release hardware
    // - Free memory
    // - Unsubscribe from events
    
    initialized_ = false;
    ESP_LOGI(TAG, "ExampleModule stopped");
}

bool ExampleModule::is_healthy() const {
    if (!initialized_) {
        return false;
    }
    
    // Check module-specific health criteria
    // For example: check if updates are happening regularly
    uint32_t now_ms = esp_timer_get_time() / 1000;
    uint32_t time_since_update = now_ms - last_update_ms_;
    
    // Consider unhealthy if no update for 5x the update interval
    return time_since_update < (update_interval_ms_ * 5);
}

uint8_t ExampleModule::get_health_score() const {
    if (!initialized_) {
        return 0;
    }
    
    // Calculate health score based on various factors
    uint8_t score = 100;
    
    // Reduce score based on time since last update
    uint32_t now_ms = esp_timer_get_time() / 1000;
    uint32_t time_since_update = now_ms - last_update_ms_;
    
    if (time_since_update > update_interval_ms_ * 2) {
        score -= 20;
    }
    if (time_since_update > update_interval_ms_ * 3) {
        score -= 30;
    }
    
    return score;
}

void ExampleModule::register_rpc(IJsonRpcRegistrar& registrar) {
    ESP_LOGI(TAG, "Registering RPC methods");
    
    // Register module-specific RPC methods
    registrar.register_method(
        "example.get_status",
        [this](const nlohmann::json& params, nlohmann::json& result) {
            return handle_rpc_status(params, result);
        },
        "Get ExampleModule status"
    );
    
    registrar.register_method(
        "example.reset_counter",
        [this](const nlohmann::json& params, nlohmann::json& result) {
            update_count_ = 0;
            result["success"] = true;
            result["message"] = "Counter reset";
            return ESP_OK;
        },
        "Reset the update counter"
    );
}

void ExampleModule::do_work() {
    // Simulate some work
    ESP_LOGD(TAG, "Doing work... (update #%lu)", update_count_);
    
    // Example: Read sensor, process data, control actuator, etc.
    // This is where your module-specific logic goes
}

esp_err_t ExampleModule::handle_rpc_status(const nlohmann::json& params, 
                                           nlohmann::json& result) {
    result["initialized"] = initialized_;
    result["update_count"] = update_count_;
    result["last_update_ms"] = last_update_ms_;
    result["update_interval_ms"] = update_interval_ms_;
    result["health_score"] = get_health_score();
    result["is_healthy"] = is_healthy();
    
    return ESP_OK;
}

// ============================================================================
// IMPORTANT: Register module with factory
// ============================================================================
// This macro registers the module with ModuleFactory, allowing it to be
// instantiated automatically based on manifest data.
//
// The name used here ("ExampleModule") must match the "name" field in your
// module_manifest.json file.

REGISTER_MODULE(ExampleModule);

// Alternative: Register with a different name
// REGISTER_MODULE_AS(ExampleModule, "CustomName");
