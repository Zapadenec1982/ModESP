/**
 * @file rev_a_refrigerator.h
 * @brief Rev A Refrigerator board configuration
 * 
 * GPIO Configuration for Industrial Refrigerator:
 * - 4x Relay outputs for compressor, fans, heater, defrost
 * - 3x Button inputs for manual control
 * - I2C for temperature sensors and display
 * - OneWire for DS18B20 temperature sensors
 */

#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// I2C port definitions
#define I2C_NUM_0 0
#define I2C_NUM_1 1

namespace BoardConfig {

// GPIO Output configurations for refrigerator control
struct GpioOutputConfig {
    const char* hal_id;
    gpio_num_t pin;
    bool active_high;
    const char* description;
};

static const GpioOutputConfig GPIO_OUTPUTS[] = {
    {"COMPRESSOR",    GPIO_NUM_1,  true,  "Compressor relay"},
    {"EVAP_FAN",      GPIO_NUM_2,  true,  "Evaporator fan"},
    {"COND_FAN",      GPIO_NUM_3,  true,  "Condenser fan"},
    {"DEFROST_HEATER", GPIO_NUM_4, true,  "Defrost heater"}
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
    {"DOOR_SWITCH",    GPIO_NUM_9,  true,  "Door open/close switch"},
    {"MANUAL_DEFROST", GPIO_NUM_10, true,  "Manual defrost button"},
    {"ALARM_RESET",    GPIO_NUM_12, true,  "Alarm reset button"}
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
    {"I2C_SENSORS", I2C_NUM_0, GPIO_NUM_15, GPIO_NUM_16, 400000, "Temperature sensors and display"}
};

static const size_t I2C_BUSES_COUNT = sizeof(I2C_BUSES) / sizeof(I2C_BUSES[0]);

// OneWire Bus configurations for temperature sensors
struct OneWireConfig {
    const char* hal_id;
    gpio_num_t data_pin;
    gpio_num_t power_pin;  // GPIO_NUM_NC if not used
    const char* description;
};

static const OneWireConfig ONEWIRE_BUSES[] = {
    {"EVAP_TEMP",      GPIO_NUM_8, GPIO_NUM_NC, "Evaporator temperature"},
    {"AMBIENT_TEMP",   GPIO_NUM_7, GPIO_NUM_NC, "Ambient temperature"},
    {"PRODUCT_TEMP",   GPIO_NUM_6, GPIO_NUM_NC, "Product temperature"}
};

static const size_t ONEWIRE_BUSES_COUNT = sizeof(ONEWIRE_BUSES) / sizeof(ONEWIRE_BUSES[0]);

// ADC Channel configurations for analog sensors
struct AdcConfig {
    const char* hal_id;
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t attenuation;
    const char* description;
};

static const AdcConfig ADC_CHANNELS[] = {
    {"PRESSURE_SENSOR", ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_12, "Pressure transducer"}
};

static const size_t ADC_CHANNELS_COUNT = sizeof(ADC_CHANNELS) / sizeof(ADC_CHANNELS[0]);

// Board-specific constants
static const char* BOARD_NAME = "Rev A Refrigerator Controller";
static const char* BOARD_VERSION = "1.0";

} // namespace BoardConfig 