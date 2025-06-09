#include "ui_manager.h"
#include "shared_state.h"
#include "config.h"
#include "event_bus.h"
#include <esp_log.h>

namespace ModuChill {

static const char* TAG = "UIManager";

UIManager* UIManager::instance_ = nullptr;

UIManager::UIManager() 
    : command_counter_(0) {
}

UIManager* UIManager::get_instance() {
    if (!instance_) {
        instance_ = new UIManager();
    }
    return instance_;
}

void UIManager::init() {
    auto* manager = get_instance();
    
    ESP_LOGI(TAG, "UIManager initialized");
    
    // Register for config change events
    EventBus::subscribe("config.changed", [](const Event& e) {
        // Copy updated config values to SharedState
        auto config = Config::get_all();
        SharedState::set("runtime.setpoint", config["climate"]["setpoint"].get<float>());
        SharedState::set("runtime.hysteresis", config["climate"]["hysteresis"].get<float>());
    });
    
    // Initialize runtime values from config
    auto config = Config::get_all();
    SharedState::set("runtime.setpoint", config["climate"]["setpoint"].get<float>());
    SharedState::set("runtime.hysteresis", config["climate"]["hysteresis"].get<float>());
}

json UIManager::get_ui_schema(const std::string& page) {
    auto* manager = get_instance();
    
    if (page == "main" || page == "home") {
        return manager->get_main_page_schema();
    } else if (page == "settings") {
        return manager->get_settings_page_schema();
    } else if (page == "network") {
        return manager->get_network_page_schema();
    } else if (page == "status") {
        return manager->get_status_page_schema();
    }
    
    // Default error response
    return {
        {"error", "Unknown page"},
        {"page", page}
    };
}

json UIManager::get_main_page_schema() {
    json schema = {
        {"page", "main"},
        {"title", "ModuChill Climate Control"},
        {"sections", json::array()}
    };
    
    // Current status section
    json status_section = {
        {"type", "status"},
        {"title", "Current Status"},
        {"items", json::array({
            {
                {"id", "current_temp"},
                {"label", "Temperature"},
                {"value", SharedState::get<float>("sensor.temperature").value_or(0.0)},
                {"unit", "°C"},
                {"format", "%.1f"}
            },
            {
                {"id", "setpoint"},
                {"label", "Setpoint"},
                {"value", SharedState::get<float>("runtime.setpoint").value_or(4.0)},
                {"unit", "°C"},
                {"format", "%.1f"}
            },
            {
                {"id", "compressor_state"},
                {"label", "Compressor"},
                {"value", SharedState::get<bool>("actuator.compressor").value_or(false)},
                {"type", "boolean"},
                {"true_text", "ON"},
                {"false_text", "OFF"},
                {"true_color", "green"},
                {"false_color", "gray"}
            }
        })}
    };
    
    // Control section
    json control_section = {
        {"type", "controls"},
        {"title", "Temperature Control"},
        {"items", json::array({
            {
                {"id", "setpoint_adjust"},
                {"type", "slider"},
                {"label", "Setpoint"},
                {"value", SharedState::get<float>("runtime.setpoint").value_or(4.0)},
                {"min", -10.0},
                {"max", 10.0},
                {"step", 0.5},
                {"unit", "°C"},
                {"action", {
                    {"type", "set_value"},
                    {"target", "runtime.setpoint"}
                }}
            }
        })}
    };
    
    schema["sections"].push_back(status_section);
    schema["sections"].push_back(control_section);
    
    return schema;
}