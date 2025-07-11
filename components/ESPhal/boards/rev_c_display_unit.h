/**
 * @file rev_c_display_unit.h
 * @brief Rev C Display Unit board configuration
 * 
 * GPIO Configuration for Display/Control Unit:
 * - 2x Relay outputs for alarms
 * - 8x Button inputs for user interface
 * - I2C for large display and sensors
 * - SPI for additional displays/interfaces
 */

#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// I2C port definitions
#define I2C_NUM_0 0
#define I2C_NUM_1 1

namespace BoardConfig {

// GPIO Output configurations for display unit
struct GpioOutputConfig {
    const char* hal_id;
    gpio_num_t pin;
    bool active_high;
    const char* description;
};

static const GpioOutputConfig GPIO_OUTPUTS[] = {
    {"ALARM_BUZZER",    GPIO_NUM_1, true, "Audible alarm"},
    {"STATUS_LED",      GPIO_NUM_2, true, "Status indicator LED"}
};

static const size_t GPIO_OUTPUTS_COUNT = sizeof(GPIO_OUTPUTS) / sizeof(GPIO_OUTPUTS[0]);

// GPIO Input configurations  
struct GpioInputConfig {
    const char* hal_id;
    gpio_num_t pin;
    bool pull_up;
    const char* description;
};

static const GpioInputConfig GPIO_INPUTS[] = {
    {"UP_BUTTON",       GPIO_NUM_9,  true, "Navigation up"},
    {"DOWN_BUTTON",     GPIO_NUM_10, true, "Navigation down"},
    {"LEFT_BUTTON",     GPIO_NUM_11, true, "Navigation left"},
    {"RIGHT_BUTTON",    GPIO_NUM_12, true, "Navigation right"},
    {"ENTER_BUTTON",    GPIO_NUM_13, true, "Confirm/Enter"},
    {"BACK_BUTTON",     GPIO_NUM_14, true, "Back/Cancel"},
    {"MENU_BUTTON",     GPIO_NUM_21, true, "Main menu"},
    {"ALARM_ACK",       GPIO_NUM_47, true, "Alarm acknowledge"}
};

static const size_t GPIO_INPUTS_COUNT = sizeof(GPIO_INPUTS) / sizeof(GPIO_INPUTS[0]);

// I2C Bus configurations
struct I2CConfig {
    const char* hal_id;
    int port;
    gpio_num_t scl_pin;
    gpio_num_t sda_pin;
    uint32_t frequency;
    const char* description;
};

static const I2CConfig I2C_BUSES[] = {
    {"I2C_DISPLAY", I2C_NUM_0, GPIO_NUM_15, GPIO_NUM_16, 400000, "Main display interface"}
};

static const size_t I2C_BUSES_COUNT = sizeof(I2C_BUSES) / sizeof(I2C_BUSES[0]);

// OneWire Bus configurations (minimal for display unit)
struct OneWireConfig {
    const char* hal_id;
    gpio_num_t data_pin;
    gpio_num_t power_pin;
    const char* description;
};

static const OneWireConfig ONEWIRE_BUSES[] = {
    {"LOCAL_TEMP", GPIO_NUM_8, GPIO_NUM_NC, "Local ambient temperature"}
};

static const size_t ONEWIRE_BUSES_COUNT = sizeof(ONEWIRE_BUSES) / sizeof(ONEWIRE_BUSES[0]);

// ADC Channel configurations
struct AdcConfig {
    const char* hal_id;
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t attenuation;
    const char* description;
};

static const AdcConfig ADC_CHANNELS[] = {
    {"AMBIENT_LIGHT", ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_12, "Ambient light sensor"}
};

static const size_t ADC_CHANNELS_COUNT = sizeof(ADC_CHANNELS) / sizeof(ADC_CHANNELS[0]);

// Board-specific constants
static const char* BOARD_NAME = "Rev C Display Unit";
static const char* BOARD_VERSION = "3.0";

} // namespace BoardConfig 