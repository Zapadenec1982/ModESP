/**
 * @file actuator_module.cpp
 * @brief Implementation of ActuatorModule with modular driver architecture
 * 
 * ActuatorModule manages actuators through a driver registry system where 
 * each actuator type is a self-contained driver component.
 */

#include "actuator_module.h"
#include "actuator_driver_registry.h"
#include "shared_state.h"
#include "event_bus.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <algorithm>

static const char* TAG = "ActuatorModule";

// === ActuatorModule implementation ===

ActuatorModule::ActuatorModule(ESPhal& hal) 
    : hal_(hal) {
    ESP_LOGI(TAG, "ActuatorModule created");
}

esp_err_t ActuatorModule::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "ActuatorModule already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing ActuatorModule...");    
    // Log available drivers
    auto& registry = ActuatorDriverRegistry::instance();
    auto available_drivers = registry.get_registered_types();
    
    ESP_LOGI(TAG, "Available actuator drivers: %zu", available_drivers.size());
    for (const auto& driver_type : available_drivers) {
        ESP_LOGI(TAG, "  - %s", driver_type.c_str());
    }
    
    // Configuration will be loaded later via configure()
    initialized_ = true;
    
    ESP_LOGI(TAG, "ActuatorModule initialized successfully");
    return ESP_OK;
}

void ActuatorModule::configure(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Configuring ActuatorModule");
    
    // Clear existing actuators
    for (auto& actuator : actuators_) {
        if (!actuator.subscription_id.empty()) {
            SharedState::unsubscribe(actuator.subscription_id);
        }
    }
    actuators_.clear();
    
    // Parse global settings
    if (config.contains("update_interval_ms")) {
        update_interval_ms_ = config["update_interval_ms"].get<uint32_t>();
        ESP_LOGI(TAG, "Update interval: %u ms", update_interval_ms_);
    }
    
    if (config.contains("publish_on_error")) {
        publish_on_error_ = config["publish_on_error"].get<bool>();
    }    
    // Create actuators from configuration
    if (config.contains("actuators")) {
        const auto& actuators_array = config["actuators"];
        
        for (const auto& actuator_config : actuators_array) {
            esp_err_t ret = create_actuator_from_config(actuator_config);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to create actuator");
            }
        }
    }
    
    ESP_LOGI(TAG, "Configured %zu actuators", actuators_.size());
}

esp_err_t ActuatorModule::create_actuator_from_config(const nlohmann::json& actuator_config) {
    try {
        // Parse actuator configuration
        ActuatorConfig config;
        config.role = actuator_config["role"].get<std::string>();
        config.type = actuator_config["type"].get<std::string>();
        config.command_key = actuator_config["command_key"].get<std::string>();
        config.status_key = actuator_config["status_key"].get<std::string>();
        config.config = actuator_config.value("config", nlohmann::json::object());
        
        ESP_LOGI(TAG, "Creating actuator: role='%s', type='%s'", 
                 config.role.c_str(), config.type.c_str());
        
        // Create driver through registry
        auto& registry = ActuatorDriverRegistry::instance();
        auto driver = registry.create_driver(config.type);        
        if (!driver) {
            ESP_LOGE(TAG, "Unknown actuator type: %s", config.type.c_str());
            return ESP_ERR_NOT_FOUND;
        }
        
        // Initialize driver with HAL and config
        esp_err_t ret = driver->init(hal_, config.config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize driver: %s", esp_err_to_name(ret));
            return ret;
        }
        
        // Create actuator instance
        ActuatorInstance instance;
        instance.driver = std::move(driver);
        instance.config = config;
        
        // Subscribe to command key
        instance.subscription_id = SharedState::subscribe(
            config.command_key,
            [this, role = config.role](const std::string& key, const nlohmann::json& value) {
                this->handle_command(role, value);
            }
        );
        
        actuators_.push_back(std::move(instance));
        
        // Publish initial status
        publish_actuator_status(actuators_.back());
        
        ESP_LOGI(TAG, "Actuator created successfully: %s", config.role.c_str());
        return ESP_OK;
        
    } catch (const nlohmann::json::exception& e) {
        ESP_LOGE(TAG, "JSON parsing error: %s", e.what());
        return ESP_ERR_INVALID_ARG;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Error creating actuator: %s", e.what());
        return ESP_FAIL;
    }
}
void ActuatorModule::handle_command(const std::string& role, const nlohmann::json& value) {
    // Find actuator by role
    auto it = std::find_if(actuators_.begin(), actuators_.end(),
        [&role](const ActuatorInstance& actuator) {
            return actuator.config.role == role;
        });
    
    if (it == actuators_.end()) {
        ESP_LOGE(TAG, "Actuator not found: %s", role.c_str());
        return;
    }
    
    // Execute command
    esp_err_t ret = it->driver->execute_command(value);
    it->command_count++;
    total_commands_++;
    
    if (ret != ESP_OK) {
        it->error_count++;
        total_errors_++;
        ESP_LOGE(TAG, "Command execution failed for %s: %s", 
                 role.c_str(), esp_err_to_name(ret));
    }
    
    // Publish updated status
    publish_actuator_status(*it);
    
    // Publish event
    nlohmann::json event_data = {
        {"role", role},
        {"command", value},
        {"success", ret == ESP_OK}
    };
    EventBus::publish("actuator.command", event_data);
}

void ActuatorModule::update() {
    if (!initialized_) {
        return;
    }
    
    update_count_++;
    
    // Update all actuators (allows time-based operations like ramping)
    for (auto& actuator : actuators_) {
        try {
            actuator.driver->update();
            
            // Periodically publish status
            if (update_count_ % (1000 / update_interval_ms_) == 0) {
                publish_actuator_status(actuator);
            }
            
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Exception updating actuator %s: %s", 
                     actuator.config.role.c_str(), e.what());
            actuator.error_count++;
            total_errors_++;
        }
    }
}
void ActuatorModule::publish_actuator_status(const ActuatorInstance& actuator) {
    // Get current status from driver
    ActuatorStatus status = actuator.driver->get_status();
    
    // Add metadata
    nlohmann::json status_json = status.to_json();
    status_json["role"] = actuator.config.role;
    status_json["type"] = actuator.config.type;
    status_json["command_count"] = actuator.command_count;
    status_json["error_count"] = actuator.error_count;
    
    // Publish to SharedState
    SharedState::set(actuator.config.status_key, status_json);
}

void ActuatorModule::stop() {
    ESP_LOGI(TAG, "Stopping ActuatorModule");
    
    // Emergency stop all actuators
    emergency_stop_all();
    
    // Unsubscribe from SharedState
    for (auto& actuator : actuators_) {
        if (!actuator.subscription_id.empty()) {
            SharedState::unsubscribe(actuator.subscription_id);
        }
    }
    
    // Clear all actuators
    actuators_.clear();
    
    initialized_ = false;
}

bool ActuatorModule::is_healthy() const {
    if (!initialized_) {
        return false;
    }
    
    // Check if any actuator has too many errors
    for (const auto& actuator : actuators_) {
        if (actuator.error_count > 10) {
            return false;
        }
        if (!actuator.driver->is_available()) {
            return false;
        }
    }
    
    return true;
}
uint8_t ActuatorModule::get_health_score() const {
    if (!initialized_ || actuators_.empty()) {
        return 0;
    }
    
    // Calculate health based on error rate
    if (total_commands_ == 0) {
        return 100;
    }
    
    float error_rate = (float)total_errors_ / total_commands_;
    return (uint8_t)((1.0f - error_rate) * 100);
}

uint32_t ActuatorModule::get_max_update_time_us() const {
    // Estimate based on actuator count
    return actuators_.size() * 5000; // 5ms per actuator
}

std::optional<ActuatorStatus> ActuatorModule::get_actuator_status(const std::string& role) const {
    auto it = std::find_if(actuators_.begin(), actuators_.end(),
        [&role](const ActuatorInstance& actuator) {
            return actuator.config.role == role;
        });
    
    if (it != actuators_.end()) {
        return it->driver->get_status();
    }
    
    return std::nullopt;
}

std::optional<ActuatorConfig> ActuatorModule::get_actuator_config(const std::string& role) const {
    auto it = std::find_if(actuators_.begin(), actuators_.end(),
        [&role](const ActuatorInstance& actuator) {
            return actuator.config.role == role;
        });
    
    if (it != actuators_.end()) {
        return it->config;
    }
    
    return std::nullopt;
}
esp_err_t ActuatorModule::execute_command(const std::string& role, const nlohmann::json& command) {
    auto it = std::find_if(actuators_.begin(), actuators_.end(),
        [&role](const ActuatorInstance& actuator) {
            return actuator.config.role == role;
        });
    
    if (it == actuators_.end()) {
        ESP_LOGE(TAG, "Actuator not found: %s", role.c_str());
        return ESP_ERR_NOT_FOUND;
    }
    
    // Execute command directly
    esp_err_t ret = it->driver->execute_command(command);
    
    // Update statistics
    it->command_count++;
    total_commands_++;
    
    if (ret != ESP_OK) {
        it->error_count++;
        total_errors_++;
    }
    
    // Publish updated status
    publish_actuator_status(*it);
    
    return ret;
}

esp_err_t ActuatorModule::emergency_stop_all() {
    ESP_LOGW(TAG, "Emergency stop all actuators");
    
    esp_err_t overall_result = ESP_OK;
    
    for (auto& actuator : actuators_) {
        esp_err_t ret = actuator.driver->emergency_stop();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to emergency stop %s: %s", 
                     actuator.config.role.c_str(), esp_err_to_name(ret));
            overall_result = ret;
        }
        
        // Publish status after emergency stop
        publish_actuator_status(actuator);
    }
    
    // Publish event
    EventBus::publish("actuator.emergency_stop", nlohmann::json::object());
    
    return overall_result;
}

std::vector<std::string> ActuatorModule::get_available_drivers() const {
    return ActuatorDriverRegistry::instance().get_registered_types();
}