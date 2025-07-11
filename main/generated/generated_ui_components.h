// generated_ui_components.h
// AUTO-GENERATED - DO NOT EDIT
#pragma once

#include "ui_component_base.h"
#include <array>

namespace ModESP::UI {

// Component metadata
struct ComponentInfo {
    const char* id;
    ComponentType type;
    const char* condition;
    AccessLevel min_access;
    Priority priority;
    bool lazy_loadable;
    const char* source;
};

// All possible components
constexpr ComponentInfo ALL_COMPONENTS[] = {
    {
        "sensor_overview",
        ComponentType::COMPOSITE,
        "always",
        AccessLevel::USER,
        Priority::HIGH,
        false,
        "SensorManager"
    },
    {
        "sensor_list",
        ComponentType::LIST,
        "always",
        AccessLevel::USER,
        Priority::HIGH,
        true,
        "SensorManager"
    },
    {
        "sensor_config_panel",
        ComponentType::COMPOSITE,
        "always",
        AccessLevel::TECHNICIAN,
        Priority::MEDIUM,
        true,
        "SensorManager"
    },
    {
        "calibration_button",
        ComponentType::BUTTON,
        "always",
        AccessLevel::TECHNICIAN,
        Priority::LOW,
        true,
        "SensorManager"
    },

};

constexpr size_t COMPONENT_COUNT = 4;

} // namespace ModESP::UI
