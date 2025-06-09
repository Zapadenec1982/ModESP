/**
 * @file esphal.cpp
 * @brief Implementation of ESPhal - Hardware Abstraction Layer
 */

#include "esphal.h"
#include "board_config.h"
#include <esp_log.h>
#include <stdexcept>

static const char* TAG = "ESPhal";

esp_err_t ESPhal::init() {
    if (initialized_) {
        ESP_LOGW(TAG, "ESPhal already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing ESPhal for %s", BoardConfig::BOARD_NAME);
    
    esp_err_t ret;
    
    // Initialize all hardware resources eagerly
    ret = init_gpio_outputs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GPIO outputs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_gpio_inputs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize GPIO inputs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_onewire_buses();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize OneWire buses: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = init_adc_channels();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize ADC channels: %s", esp_err_to_name(ret));
        return ret;
    }
    
    initialized_ = true;
    
    ESP_LOGI(TAG, "ESPhal initialized successfully:");
    ESP_LOGI(TAG, "  - GPIO outputs: %zu", gpio_outputs_.size());
    ESP_LOGI(TAG, "  - GPIO inputs: %zu", gpio_inputs_.size());
    ESP_LOGI(TAG, "  - OneWire buses: %zu", onewire_buses_.size());
    ESP_LOGI(TAG, "  - ADC channels: %zu", adc_channels_.size());
    
    return ESP_OK;
}

// Resource access methods with error checking
IGpioOutput& ESPhal::get_gpio_output(const std::string& hal_id) {
    auto it = gpio_outputs_.find(hal_id);
    if (it == gpio_outputs_.end()) {
        throw std::runtime_error("GPIO output '" + hal_id + "' not found. Check board configuration.");
    }
    return *it->second;
}

IGpioInput& ESPhal::get_gpio_input(const std::string& hal_id) {
    auto it = gpio_inputs_.find(hal_id);
    if (it == gpio_inputs_.end()) {
        throw std::runtime_error("GPIO input '" + hal_id + "' not found. Check board configuration.");
    }
    return *it->second;
}

IOneWireBus& ESPhal::get_onewire_bus(const std::string& hal_id) {
    auto it = onewire_buses_.find(hal_id);
    if (it == onewire_buses_.end()) {
        throw std::runtime_error("OneWire bus '" + hal_id + "' not found. Check board configuration.");
    }
    return *it->second;
}

IAdcChannel& ESPhal::get_adc_channel(const std::string& hal_id) {
    auto it = adc_channels_.find(hal_id);
    if (it == adc_channels_.end()) {
        throw std::runtime_error("ADC channel '" + hal_id + "' not found. Check board configuration.");
    }
    return *it->second;
}

// Resource existence checking methods
bool ESPhal::has_gpio_output(const std::string& hal_id) const {
    return gpio_outputs_.find(hal_id) != gpio_outputs_.end();
}

bool ESPhal::has_gpio_input(const std::string& hal_id) const {
    return gpio_inputs_.find(hal_id) != gpio_inputs_.end();
}

bool ESPhal::has_onewire_bus(const std::string& hal_id) const {
    return onewire_buses_.find(hal_id) != onewire_buses_.end();
}

bool ESPhal::has_adc_channel(const std::string& hal_id) const {
    return adc_channels_.find(hal_id) != adc_channels_.end();
}

std::string ESPhal::get_board_info() const {
    return std::string(BoardConfig::BOARD_NAME) + " v" + std::string(BoardConfig::BOARD_VERSION);
}

// Helper initialization methods
esp_err_t ESPhal::init_gpio_outputs() {
    ESP_LOGI(TAG, "Initializing GPIO outputs...");
    
    for (size_t i = 0; i < BoardConfig::GPIO_OUTPUTS_COUNT; i++) {
        const auto& config = BoardConfig::GPIO_OUTPUTS[i];
        
        ESP_LOGD(TAG, "  Creating GPIO output: %s on pin %d", config.hal_id, config.pin);
        
        // TODO: Create actual GpioOutputImpl when we implement it
        // For now, this is a placeholder to verify the structure works
        // auto gpio_output = std::make_unique<GpioOutputImpl>(config.pin, config.active_high);
        // gpio_outputs_[config.hal_id] = std::move(gpio_output);
        
        ESP_LOGD(TAG, "    %s - Pin: %d, Active: %s", 
                config.hal_id, config.pin, config.active_high ? "HIGH" : "LOW");
    }
    
    ESP_LOGI(TAG, "GPIO outputs initialized: %zu outputs", BoardConfig::GPIO_OUTPUTS_COUNT);
    return ESP_OK;
}

esp_err_t ESPhal::init_gpio_inputs() {
    ESP_LOGI(TAG, "Initializing GPIO inputs...");
    
    for (size_t i = 0; i < BoardConfig::GPIO_INPUTS_COUNT; i++) {
        const auto& config = BoardConfig::GPIO_INPUTS[i];
        
        ESP_LOGD(TAG, "  Creating GPIO input: %s on pin %d", config.hal_id, config.pin);
        
        // TODO: Create actual GpioInputImpl when we implement it
        // auto gpio_input = std::make_unique<GpioInputImpl>(config.pin, config.pull_up);
        // gpio_inputs_[config.hal_id] = std::move(gpio_input);
        
        ESP_LOGD(TAG, "    %s - Pin: %d, Pull-up: %s", 
                config.hal_id, config.pin, config.pull_up ? "YES" : "NO");
    }
    
    ESP_LOGI(TAG, "GPIO inputs initialized: %zu inputs", BoardConfig::GPIO_INPUTS_COUNT);
    return ESP_OK;
}

esp_err_t ESPhal::init_onewire_buses() {
    ESP_LOGI(TAG, "Initializing OneWire buses...");
    
    for (size_t i = 0; i < BoardConfig::ONEWIRE_BUSES_COUNT; i++) {
        const auto& config = BoardConfig::ONEWIRE_BUSES[i];
        
        ESP_LOGD(TAG, "  Creating OneWire bus: %s on pin %d", config.hal_id, config.data_pin);
        
        // TODO: Create actual OneWireBusImpl when we implement it
        // auto onewire_bus = std::make_unique<OneWireBusImpl>(config.data_pin, config.power_pin);
        // onewire_buses_[config.hal_id] = std::move(onewire_bus);
        
        ESP_LOGD(TAG, "    %s - Data pin: %d, Power pin: %d", 
                config.hal_id, config.data_pin, 
                config.power_pin == GPIO_NUM_NC ? -1 : config.power_pin);
    }
    
    ESP_LOGI(TAG, "OneWire buses initialized: %zu buses", BoardConfig::ONEWIRE_BUSES_COUNT);
    return ESP_OK;
}

esp_err_t ESPhal::init_adc_channels() {
    ESP_LOGI(TAG, "Initializing ADC channels...");
    
    for (size_t i = 0; i < BoardConfig::ADC_CHANNELS_COUNT; i++) {
        const auto& config = BoardConfig::ADC_CHANNELS[i];
        
        ESP_LOGD(TAG, "  Creating ADC channel: %s on ADC%d_CH%d", 
                config.hal_id, config.unit, config.channel);
        
        // TODO: Create actual AdcChannelImpl when we implement it
        // auto adc_channel = std::make_unique<AdcChannelImpl>(config.unit, config.channel, config.attenuation);
        // adc_channels_[config.hal_id] = std::move(adc_channel);
        
        ESP_LOGD(TAG, "    %s - Unit: %d, Channel: %d, Atten: %d", 
                config.hal_id, config.unit, config.channel, config.attenuation);
    }
    
    ESP_LOGI(TAG, "ADC channels initialized: %zu channels", BoardConfig::ADC_CHANNELS_COUNT);
    return ESP_OK;
}
