// Auto-generated LCD menu structure
// Generated at: 2025-06-25T22:14:58.436291

#pragma once

#include <pgmspace.h>

enum MenuItemType {
    MENU_TYPE_VALUE,
    MENU_TYPE_SUBMENU,
    MENU_TYPE_ACTION,
    MENU_TYPE_BACK
};

struct MenuItem {
    const char* label;
    MenuItemType type;
    union {
        const char* state_key;     // for values
        uint8_t submenu_id;        // for submenus
        uint8_t action_id;         // for actions
    } data;
};

// sensor_drivers submenu
const MenuItem SUBMENU_1[] PROGMEM = {
    {"Temperature", MENU_TYPE_VALUE, {.state_key = "sensor_drivers.temperature"}},
    {"Humidity", MENU_TYPE_VALUE, {.state_key = "sensor_drivers.humidity"}},
    {"Temp Calibration", MENU_TYPE_VALUE, {.state_key = "sensor_drivers.temp_offset"}},
    {"Back", MENU_TYPE_BACK, {0}},
};

// Main menu
const MenuItem MAIN_MENU[] PROGMEM = {
    {"Sensors", MENU_TYPE_SUBMENU, {.submenu_id = 1}},
};

const size_t MAIN_MENU_SIZE = sizeof(MAIN_MENU) / sizeof(MenuItem);
