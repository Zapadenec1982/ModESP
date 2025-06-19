/**
 * @file i2c_interfaces.h
 * @brief I2C interfaces for HAL
 */

#pragma once

#include <cstdint>
#include <vector>
#include "esp_err.h"
#include "driver/i2c.h"

/**
 * @brief I2C Bus interface
 */
class II2CBus {
public:
    virtual ~II2CBus() = default;
    
    /**
     * @brief Write data to I2C device
     * @param device_addr 7-bit device address
     * @param data Data to write
     * @param len Length of data
     * @return ESP_OK on success
     */
    virtual esp_err_t write(uint8_t device_addr, const uint8_t* data, size_t len) = 0;
    
    /**
     * @brief Read data from I2C device
     * @param device_addr 7-bit device address
     * @param data Buffer to store read data
     * @param len Length to read
     * @return ESP_OK on success
     */
    virtual esp_err_t read(uint8_t device_addr, uint8_t* data, size_t len) = 0;
    
    /**
     * @brief Write to register and read data
     * @param device_addr 7-bit device address
     * @param reg_addr Register address
     * @param data Buffer to store read data
     * @param len Length to read
     * @return ESP_OK on success
     */
    virtual esp_err_t write_read(uint8_t device_addr, uint8_t reg_addr, uint8_t* data, size_t len) = 0;
    
    /**
     * @brief Scan I2C bus for devices
     * @return Vector of found device addresses
     */
    virtual std::vector<uint8_t> scan() = 0;
};
