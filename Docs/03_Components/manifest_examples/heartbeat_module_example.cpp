/**
 * @file heartbeat_module_example.cpp
 * @brief Example of HeartbeatModule using manifest-driven architecture
 * 
 * This shows how an existing module can be updated to use:
 * - Type-safe event publishing
 * - Module factory registration
 * - Manifest-based configuration
 */

#include "module_heartbeat.h"
#include "module_factory.h"
#include "event_helpers.h"
#include "module_manager.h"
#include "esp_log.h"
#include "esp_timer.h"

using namespace ModESP;

static const char* TAG = "HeartbeatModule";

// Original ModuleHeartbeat implementation with manifest integration
void ModuleHeartbeat::update() {
    if (!initialized_) {
        return;
    }
    
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_heartbeat_ms_ < heartbeat_interval_ms_) {
        return;
    }
    
    last_heartbeat_ms_ = now;
    
    // Get system health report
    auto health_report = ModuleManager::get_health_report();
    
    // Use type-safe event publisher
    EventPublisher::publishSystemHeartbeat(
        health_report.total_modules,
        health_report.system_health_score,
        now / 1000  // uptime in seconds
    );
    
    // Check for health warnings
    for (const auto& module : health_report.modules) {
        if (module.health_score < warning_threshold_ && module.enabled) {
            EventPublisher::publishSystemHealthWarning(
                module.name,
                module.health_score,
                "Module health below threshold"
            );
        }
    }
    
    ESP_LOGD(TAG, "Heartbeat: %zu modules, health: %d%%", 
             health_report.total_modules, 
             health_report.system_health_score);
}

// Example of subscribing to events with type safety
void ModuleHeartbeat::init() {
    if (initialized_) {
        return;
    }
    
    ESP_LOGI(TAG, "Initializing HeartbeatModule");
    
    // Subscribe to health warnings using type-safe subscriber
    warning_subscription_ = EventSubscriber::onHealthWarning(
        [this](const std::string& module, uint8_t score, const std::string& reason) {
            ESP_LOGW(TAG, "Health warning from %s: score=%d, reason=%s", 
                     module.c_str(), score, reason.c_str());
            
            // Track warning history
            warning_history_[module] = {score, reason, esp_timer_get_time()};
        });
    
    // Subscribe to sensor errors
    error_subscription_ = EventSubscriber::onSensorError(
        [this](const std::string& role, const std::string& error, int code) {
            ESP_LOGE(TAG, "Sensor error: %s - %s (code: %d)", 
                     role.c_str(), error.c_str(), code);
            
            // Could trigger recovery actions here
        });
    
    initialized_ = true;
    ESP_LOGI(TAG, "HeartbeatModule initialized");
}

// Register with factory - this goes at the end of the .cpp file
REGISTER_MODULE(ModuleHeartbeat);

// ============================================================================
// Alternative: Creating a completely new heartbeat module
// ============================================================================

class AdvancedHeartbeatModule : public BaseModule {
public:
    const char* get_name() const override { return "AdvancedHeartbeatModule"; }
    
    esp_err_t init() override {
        ESP_LOGI(TAG, "Initializing AdvancedHeartbeatModule");
        
        // Use compile-time checked event names
        subscription_ = SUBSCRIBE_EVENT(SYSTEM_HEALTH_WARNING, 
            [this](const EventBus::Event& event) {
                handleHealthWarning(event);
            });
        
        initialized_ = true;
        return ESP_OK;
    }
    
    void update() override {
        if (!initialized_) return;
        
        // Publish using macro for compile-time checking
        nlohmann::json data = {
            {"advanced_metrics", getAdvancedMetrics()},
            {"timestamp", esp_timer_get_time()}
        };
        
        PUBLISH_EVENT(SYSTEM_HEARTBEAT, data);
    }
    
    void stop() override {
        if (subscription_ != 0) {
            EventBus::unsubscribe(subscription_);
        }
        initialized_ = false;
    }
    
    bool is_healthy() const override { return initialized_; }
    uint8_t get_health_score() const override { return initialized_ ? 100 : 0; }
    
private:
    bool initialized_ = false;
    EventBus::SubscriptionHandle subscription_ = 0;
    
    void handleHealthWarning(const EventBus::Event& event) {
        // Process health warning
        auto module = event.data.value("module", "unknown");
        auto score = event.data.value("health_score", 0);
        
        ESP_LOGW(TAG, "Advanced handler: %s health = %d", 
                 module.c_str(), score);
    }
    
    nlohmann::json getAdvancedMetrics() {
        // Collect advanced system metrics
        return {
            {"free_heap", esp_get_free_heap_size()},
            {"min_free_heap", esp_get_minimum_free_heap_size()},
            {"task_count", uxTaskGetNumberOfTasks()}
        };
    }
};

// Register the advanced version
REGISTER_MODULE_AS(AdvancedHeartbeatModule, "AdvancedHeartbeatModule");
