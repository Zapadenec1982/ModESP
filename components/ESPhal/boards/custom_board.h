/**
 * @file custom_board.h
 * @brief Custom board configuration
 * 
 * GPIO Configuration:
 * - 4x Relay outputs (GPIO 1-4)
 * - 5x Button inputs (GPIO 9-13)
 * - I2C OLED Display (GPIO 15-16)
 * - 2x DS18B20 sensors (GPIO 7-8)
 */

#pragma once

#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"

// I2C port definitions
#define I2C_NUM_0 0
#define I2C_NUM_1 1

namespace BoardConfig {

// GPIO Output configurations
struct GpioOutputConfig {
    const char* hal_id;
    gpio_num_t pin;
    bool active_high;
    const char* description;
};

static const GpioOutputConfig GPIO_OUTPUTS[] = {
    {"RELAY_1",    GPIO_NUM_1,  true,  "Relay 1"},
    {"RELAY_2",    GPIO_NUM_2,  true,  "Relay 2"},
    {"RELAY_3",    GPIO_NUM_3,  true,  "Relay 3"},
    {"RELAY_4",    GPIO_NUM_4,  true,  "Relay 4"}
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
    {"BUTTON_1",   GPIO_NUM_9,  true,  "Button 1"},
    {"BUTTON_2",   GPIO_NUM_10, true,  "Button 2"},
    {"BUTTON_3",   GPIO_NUM_12, true,  "Button 3"},
    {"BUTTON_4",   GPIO_NUM_13, true,  "Button 4"},
    {"BUTTON_5",   GPIO_NUM_11, true,  "Button 5"}
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
    {"I2C_DISPLAY", I2C_NUM_0, GPIO_NUM_15, GPIO_NUM_16, 400000, "OLED Display I2C bus"}
};

static const size_t I2C_BUSES_COUNT = sizeof(I2C_BUSES) / sizeof(I2C_BUSES[0]);

// OneWire Bus configurations (для DS18B20)
struct OneWireConfig {
    const char* hal_id;
    gpio_num_t data_pin;
    gpio_num_t power_pin;  // GPIO_NUM_NC if not used
    const char* description;
};

static const OneWireConfig ONEWIRE_BUSES[] = {
    {"ONEWIRE_BUS_1",  GPIO_NUM_8, GPIO_NUM_NC, "DS18B20 Sensor 1"},
    {"ONEWIRE_BUS_2",  GPIO_NUM_7, GPIO_NUM_NC, "DS18B20 Sensor 2"}
};

static const size_t ONEWIRE_BUSES_COUNT = sizeof(ONEWIRE_BUSES) / sizeof(ONEWIRE_BUSES[0]);

// ADC Channel configurations (якщо потрібно в майбутньому)
struct AdcConfig {
    const char* hal_id;
    adc_unit_t unit;
    adc_channel_t channel;
    adc_atten_t attenuation;
    const char* description;
};

static const AdcConfig ADC_CHANNELS[] = {
    // Порожній масив - ADC канали не використовуються в цій конфігурації
    {"PLACEHOLDER", ADC_UNIT_1, ADC_CHANNEL_0, ADC_ATTEN_DB_11, "Placeholder ADC channel"}
};

static const size_t ADC_CHANNELS_COUNT = sizeof(ADC_CHANNELS) / sizeof(ADC_CHANNELS[0]);

// Board-specific constants
static const char* BOARD_NAME = "Custom Board";
static const char* BOARD_VERSION = "1.0";

} // namespace BoardConfig
