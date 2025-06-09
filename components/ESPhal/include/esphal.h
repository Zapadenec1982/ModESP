/**
 * @file esphal.h
 * @brief Main ESPhal class - Hardware Abstraction Layer for ModuChill devices
 * 
 * ESPhal provides unified access to hardware resources through well-defined interfaces.
 * It uses Dependency Injection pattern - modules receive a reference to ESPhal instance
 * rather than accessing it through global/static methods.
 */

#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <optional>
#include "esp_err.h"
#include "hal_interfaces.h"

/**
 * @brief Main Hardware Abstraction Layer class
 * 
 * ESPhal manages all hardware resources and provides factory methods
 * to create and access hardware drivers through standard interfaces.
 * 
 * Usage pattern:
 * 1. Create single ESPhal instance in Application::init()
 * 2. Call hal.init() to initialize all hardware buses
 * 3. Pass ESPhal reference to HAL-modules via constructor
 * 4. HAL-modules request resources: hal.get_onewire_bus("ONEWIRE_CHAMBER")
 */
class ESPhal {
public:
    /**
     * @brief Constructor
     */
    ESPhal() = default;
    
    /**
     * @brief Destructor
     */
    ~ESPhal() = default;
    
    // Non-copyable, non-movable (single instance semantics)
    ESPhal(const ESPhal&) = delete;
    ESPhal& operator=(const ESPhal&) = delete;
    ESPhal(ESPhal&&) = delete;
    ESPhal& operator=(ESPhal&&) = delete;

    /**
     * @brief Initialize HAL and all hardware buses
     * 
     * This method performs eager initialization of all hardware resources
     * defined in the board configuration. It initializes GPIO, OneWire,
     * I2C, and ADC controllers.
     * 
     * @return ESP_OK on success, error code on failure
     */
    esp_err_t init();

    /**
     * @brief Get GPIO output driver
     * 
     * @param hal_id Hardware ID from board configuration (e.g., "RELAY_COMPRESSOR")
     * @return Reference to GPIO output interface, or throws std::runtime_error if not found
     */
    IGpioOutput& get_gpio_output(const std::string& hal_id);
    
    /**
     * @brief Get GPIO input driver
     * 
     * @param hal_id Hardware ID from board configuration (e.g., "INPUT_DOOR_SWITCH")
     * @return Reference to GPIO input interface, or throws std::runtime_error if not found
     */
    IGpioInput& get_gpio_input(const std::string& hal_id);
    
    /**
     * @brief Get OneWire bus driver
     * 
     * @param hal_id Hardware ID from board configuration (e.g., "ONEWIRE_CHAMBER")
     * @return Reference to OneWire bus interface, or throws std::runtime_error if not found
     */
    IOneWireBus& get_onewire_bus(const std::string& hal_id);
    
    /**
     * @brief Get ADC channel driver
     * 
     * @param hal_id Hardware ID from board configuration (e.g., "ADC_PRESSURE_HIGH")
     * @return Reference to ADC channel interface, or throws std::runtime_error if not found
     */
    IAdcChannel& get_adc_channel(const std::string& hal_id);
    
    /**
     * @brief Check if GPIO output exists
     * @param hal_id Hardware ID to check
     * @return true if hal_id is configured as GPIO output
     */
    bool has_gpio_output(const std::string& hal_id) const;
    
    /**
     * @brief Check if GPIO input exists
     * @param hal_id Hardware ID to check  
     * @return true if hal_id is configured as GPIO input
     */
    bool has_gpio_input(const std::string& hal_id) const;
    
    /**
     * @brief Check if OneWire bus exists
     * @param hal_id Hardware ID to check
     * @return true if hal_id is configured as OneWire bus
     */
    bool has_onewire_bus(const std::string& hal_id) const;
    
    /**
     * @brief Check if ADC channel exists
     * @param hal_id Hardware ID to check
     * @return true if hal_id is configured as ADC channel
     */
    bool has_adc_channel(const std::string& hal_id) const;

    /**
     * @brief Get board information
     * @return Board name and version string
     */
    std::string get_board_info() const;

private:
    // Hardware resource maps - ESPhal owns all drivers
    std::unordered_map<std::string, std::unique_ptr<IGpioOutput>> gpio_outputs_;
    std::unordered_map<std::string, std::unique_ptr<IGpioInput>> gpio_inputs_;
    std::unordered_map<std::string, std::unique_ptr<IOneWireBus>> onewire_buses_;
    std::unordered_map<std::string, std::unique_ptr<IAdcChannel>> adc_channels_;
    
    // Initialization state
    bool initialized_ = false;
    
    // Helper methods for initialization
    esp_err_t init_gpio_outputs();
    esp_err_t init_gpio_inputs();
    esp_err_t init_onewire_buses();
    esp_err_t init_adc_channels();
    
    // Helper method for error reporting
    void throw_if_not_found(const std::string& hal_id, const std::string& resource_type) const;
};
