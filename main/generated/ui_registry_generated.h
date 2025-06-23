// Auto-generated UI registry
// Generated at: 2025-06-23T20:20:12.779419

#pragma once

#include <pgmspace.h>

// Module capabilities flags
#define UI_CAP_READ      0x01
#define UI_CAP_WRITE     0x02
#define UI_CAP_TELEMETRY 0x04
#define UI_CAP_ALARMS    0x08

struct ModuleUIInfo {
    const char* name;
    const char* label;
    uint8_t capabilities;
    uint16_t update_rate_ms;
};

const ModuleUIInfo MODULE_UI_REGISTRY[] PROGMEM = {
    {"sensor_drivers", "Sensors", 0x0F, 1000},
};

const size_t MODULE_UI_COUNT = sizeof(MODULE_UI_REGISTRY) / sizeof(ModuleUIInfo);

// Helper macros
#define MODULE_HAS_CAP(module_idx, cap) \
    (pgm_read_byte(&MODULE_UI_REGISTRY[module_idx].capabilities) & (cap))
    
#define GET_MODULE_NAME(module_idx) \
    ((const char*)pgm_read_ptr(&MODULE_UI_REGISTRY[module_idx].name))
