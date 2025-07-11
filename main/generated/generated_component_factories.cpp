// generated_component_factories.cpp
// AUTO-GENERATED - DO NOT EDIT

#include "lazy_component_loader.h"
#include "ui_component_base.h"

namespace ModESP::UI {

void registerAllComponentFactories(LazyComponentLoader& loader) {

    // sensor_overview from SensorManager
    loader.registerComponentFactory("sensor_overview", []() {
        return std::make_unique<TextComponent>(
            "sensor_overview", 
            "sensor_overview"
        );
    });

    // sensor_list from SensorManager
    loader.registerComponentFactory("sensor_list", []() {
        return std::make_unique<TextComponent>(
            "sensor_list", 
            "sensor_list"
        );
    });

    // sensor_config_panel from SensorManager
    loader.registerComponentFactory("sensor_config_panel", []() {
        return std::make_unique<TextComponent>(
            "sensor_config_panel", 
            "sensor_config_panel"
        );
    });

    // calibration_button from SensorManager
    loader.registerComponentFactory("calibration_button", []() {
        return std::make_unique<ButtonComponent>(
            "calibration_button", 
            "calibration_button"
        );
    });

}

} // namespace ModESP::UI
