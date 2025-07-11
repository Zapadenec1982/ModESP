/**
 * @file rev_b_ripening_chamber.h
 * @brief Rev B Ripening Chamber board configuration
 * 
 * GPIO Configuration for Ripening Chamber:
 * - 6x Relay outputs for ventilation, heater, humidifier, ethylene
 * - 4x Button inputs for control
 * - I2C for sensors and display
 * - OneWire for DS18B20 temperature/humidity sensors
 */

#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// I2C port definitions
#define I2C_NUM_0 0
#define I2C_NUM_1 1

namespace BoardConfig {

// GPIO Output configurations for ripening chamber control
struct GpioOutputConfig {
    const char* hal_id;
    gpio_num_t pin;
    bool active_high;
    const char* description;
};

static const GpioOutputConfig GPIO_OUTPUTS[] = {
    {"VENTILATION_FAN", GPIO_NUM_1, true,  "Ventilation fan"},
    {"HEATER",          GPIO_NUM_2, true,  "Chamber heater"},
    {"HUMIDIFIER",      GPIO_NUM_3, true,  "Humidifier"},
    {"ETHYLENE_VALVE",  GPIO_NUM_4, true,  "Ethylene gas valve"},
    {"EXHAUST_FAN",     GPIO_NUM_5, true,  "Exhaust fan"},
    {"CO2_SCRUBBER",    GPIO_NUM_6, true,  "CO2 scrubber"}
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
    {"DOOR_SWITCH",     GPIO_NUM_9,  true, "Chamber door switch"},
    {"EMERGENCY_STOP",  GPIO_NUM_10, true, "Emergency stop button"},
    {"CYCLE_START",     GPIO_NUM_11, true, "Ripening cycle start"},
    {"MANUAL_VENT",     GPIO_NUM_12, true, "Manual ventilation"}
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
    {"I2C_SENSORS", I2C_NUM_0, GPIO_NUM_15, GPIO_NUM_16, 400000, "Environmental sensors"}
};

static const size_t I2C_BUSES_COUNT = sizeof(I2C_BUSES) / sizeof(I2C_BUSES[0]);

// OneWire Bus configurations
struct OneWireConfig {
    const char* hal_id;
    gpio_num_t data_pin;
    gpio_num_t power_pin;
    const char* description;
};

static const OneWireConfig ONEWIRE_BUSES[] = {
    {"CHAMBER_TEMP",    GPIO_NUM_8, GPIO_NUM_NC, "Chamber temperature"},
    {"PRODUCT_TEMP",    GPIO_NUM_7, GPIO_NUM_NC, "Product temperature"},
    {"EXHAUST_TEMP",    GPIO_NUM_14, GPIO_NUM_NC, "Exhaust temperature"}
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
    {"HUMIDITY_SENSOR", ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_12, "Humidity sensor"},
    {"CO2_SENSOR",      ADC_UNIT_1, ADC_CHANNEL_1, ADC_ATTEN_DB_12, "CO2 concentration"},
    {"ETHYLENE_SENSOR", ADC_UNIT_1, ADC_CHANNEL_2, ADC_ATTEN_DB_12, "Ethylene concentration"}
};

static const size_t ADC_CHANNELS_COUNT = sizeof(ADC_CHANNELS) / sizeof(ADC_CHANNELS[0]);

// Board-specific constants
static const char* BOARD_NAME = "Rev B Ripening Chamber Controller";
static const char* BOARD_VERSION = "2.0";

} // namespace BoardConfig 