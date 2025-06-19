/**
 * @file onewire_impl.h
 * @brief Implementation of OneWire bus interface for DS18B20 sensors
 */

#pragma once

#include "hal_interfaces.h"
#include <driver/gpio.h>
#include <vector>
#include <cstdint>

/**
 * @brief Implementation of OneWire bus using ESP32 GPIO
 */
class OneWireBusImpl : public IOneWireBus {
public:
    /**
     * @brief Constructor
     * @param data_pin GPIO pin for OneWire data line
     * @param power_pin GPIO pin for power (GPIO_NUM_NC if not used)
     */
    OneWireBusImpl(gpio_num_t data_pin, gpio_num_t power_pin = GPIO_NUM_NC);
    
    /**
     * @brief Destructor
     */
    virtual ~OneWireBusImpl();
    
    // IOneWireBus interface implementation
    std::vector<uint64_t> search_devices() override;
    esp_err_t request_temperatures() override;
    HalResult<float> read_temperature(uint64_t address) override;

private:
    gpio_num_t data_pin_;
    gpio_num_t power_pin_;
    
    // Search state variables
    uint8_t last_discrepancy_ = 0;
    bool last_device_flag_ = false;
    uint8_t last_family_discrepancy_ = 0;
    
    // Low-level OneWire protocol methods
    bool reset();
    void write_bit(bool bit);
    bool read_bit();
    void write_byte(uint8_t byte);
    uint8_t read_byte();
    bool search_device(uint8_t* address);
    bool check_crc(const uint8_t* data, uint8_t len, uint8_t expected_crc);
};