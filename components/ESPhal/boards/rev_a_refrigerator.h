/**
 * @file rev_a_refrigerator.h
 * @brief Board configuration for Rev A Refrigerator Controller
 * * Original refrigerator controller board with basic relay and sensor support.
 * - 4x Relay outputs (compressor, fan, defrost heater, lights)
 * - 2x OneWire buses (chamber sensors, evaporator sensors)  
 * - 4x ADC channels (pressure, door sensor, spare inputs)
 * - Status LEDs
 */

#pragma once

#include "driver/gpio.h" // ДОДАНО: для визначення GPIO_NUM_xx
#include "esp_adc/adc_oneshot.h"

namespace BoardConfig {

// GPIO Output configurations
struct GpioOutputConfig {
    const char* hal_id;
    gpio_num_t pin;
    bool active_high;
    const char* description;
};

static const GpioOutputConfig GPIO_OUTPUTS[] = {
    {"RELAY_COMPRESSOR",    GPIO_NUM_4,  true,  "Main compressor relay"},
    {"RELAY_FAN",           GPIO_NUM_5,  true,  "Evaporator fan relay"},
    {"RELAY_DEFROST",       GPIO_NUM_18, true,  "Defrost heater relay"},
    {"RELAY_LIGHTS",        GPIO_NUM_19, true,  "Internal lights relay"},
    {"LED_STATUS",          GPIO_NUM_2,  false, "Status LED (built-in)"},
    {"LED_ALARM",           GPIO_NUM_21, false, "Alarm indicator LED"}
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
    {"INPUT_DOOR_SWITCH",   GPIO_NUM_15, true,  "Door open/close switch"},
    {"INPUT_DEFROST_END",   GPIO_NUM_16, true,  "Defrost end switch"},
    {"INPUT_EMERGENCY",     GPIO_NUM_17, true,  "Emergency stop button"}
};

static const size_t GPIO_INPUTS_COUNT = sizeof(GPIO_INPUTS) / sizeof(GPIO_INPUTS[0]);

// OneWire Bus configurations
struct OneWireConfig {
    const char* hal_id;
    gpio_num_t data_pin;
    gpio_num_t power_pin;  // GPIO_NUM_NC if not used
    const char* description;
};

static const OneWireConfig ONEWIRE_BUSES[] = {
    {"ONEWIRE_CHAMBER",     GPIO_NUM_32, GPIO_NUM_33, "Chamber temperature sensors"},
    {"ONEWIRE_EVAPORATOR",  GPIO_NUM_14, GPIO_NUM_NC, "Evaporator temperature sensors"}
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
    {"ADC_PRESSURE_HIGH",   ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_12, "High pressure sensor"},
    {"ADC_PRESSURE_LOW",    ADC_UNIT_1, ADC_CHANNEL_1, ADC_ATTEN_DB_12, "Low pressure sensor"},
    {"ADC_AMBIENT_TEMP",    ADC_UNIT_1, ADC_CHANNEL_2, ADC_ATTEN_DB_12, "Ambient temperature (NTC)"},
    {"ADC_SPARE_INPUT",     ADC_UNIT_1, ADC_CHANNEL_3, ADC_ATTEN_DB_12, "Spare analog input"}
};

static const size_t ADC_CHANNELS_COUNT = sizeof(ADC_CHANNELS) / sizeof(ADC_CHANNELS[0]);

// Board-specific constants
static const char* BOARD_NAME = "Rev A Refrigerator Controller";
static const char* BOARD_VERSION = "1.0";

} // namespace BoardConfig