/**
 * @file esphal.cpp
 * @brief Implementation of ESPhal - Hardware Abstraction Layer
 */

#include "esphal.h"
#include "board_config.h"
#include <esp_log.h>
#include <cstring>
#include <stdexcept>
#include <map>
#include "esp_mac.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

static_assert(sizeof(BoardConfig::BOARD_NAME) > 1, 
    "BoardConfig::BOARD_NAME must be defined in the board configuration file.");
static_assert(sizeof(BoardConfig::GPIO_OUTPUTS) > 0, 
    "BoardConfig::GPIO_OUTPUTS array must be defined in the board configuration file.");
static_assert(sizeof(BoardConfig::GPIO_INPUTS) > 0, 
    "BoardConfig::GPIO_INPUTS array must be defined in the board configuration file.");
static_assert(sizeof(BoardConfig::ONEWIRE_BUSES) > 0, 
    "BoardConfig::ONEWIRE_BUSES array must be defined in the board configuration file.");
static_assert(sizeof(BoardConfig::ADC_CHANNELS) > 0, 
    "BoardConfig::ADC_CHANNELS array must be defined in the board configuration file.");

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
    
    // Log available ADC channels
    if (!adc_channels_.empty()) {
        ESP_LOGI(TAG, "Available ADC channels:");
        for (const auto& [id, channel] : adc_channels_) {
            ESP_LOGI(TAG, "    - %s", id.c_str());
        }
    }
    
    return ESP_OK;
}

// Resource access methods with error checking
IGpioOutput& ESPhal::get_gpio_output(const std::string& hal_id) {
    auto it = gpio_outputs_.find(hal_id);
    if (it == gpio_outputs_.end()) {
        ESP_LOGE(TAG, "GPIO output '%s' not found. Check board configuration.", hal_id.c_str());
        // Return a null reference - this will cause a crash, but it's better than exceptions
        // In production code, we should return a Result<IGpioOutput&> instead
        static IGpioOutput* null_output = nullptr;
        return *null_output; // This will crash, but no exceptions
    }
    return *it->second;
}

IGpioInput& ESPhal::get_gpio_input(const std::string& hal_id) {
    auto it = gpio_inputs_.find(hal_id);
    if (it == gpio_inputs_.end()) {
        ESP_LOGE(TAG, "GPIO input '%s' not found. Check board configuration.", hal_id.c_str());
        static IGpioInput* null_input = nullptr;
        return *null_input; // This will crash, but no exceptions
    }
    return *it->second;
}

IOneWireBus& ESPhal::get_onewire_bus(const std::string& hal_id) {
    auto it = onewire_buses_.find(hal_id);
    if (it == onewire_buses_.end()) {
        ESP_LOGE(TAG, "OneWire bus '%s' not found. Check board configuration.", hal_id.c_str());
        static IOneWireBus* null_bus = nullptr;
        return *null_bus; // This will crash, but no exceptions
    }
    return *it->second;
}

IOneWireBus* ESPhal::get_onewire_bus_ptr(const std::string& hal_id) {
    auto it = onewire_buses_.find(hal_id);
    if (it == onewire_buses_.end()) {
        return nullptr;
    }
    return it->second.get();
}

IAdcChannel& ESPhal::get_adc_channel(const std::string& hal_id) {
    auto it = adc_channels_.find(hal_id);
    if (it == adc_channels_.end()) {
        ESP_LOGE(TAG, "ADC channel '%s' not found. Check board configuration.", hal_id.c_str());
        static IAdcChannel* null_channel = nullptr;
        return *null_channel; // This will crash, but no exceptions
    }
    return *it->second;
}

IAdcChannel* ESPhal::get_adc_channel_ptr(const std::string& hal_id) {
    auto it = adc_channels_.find(hal_id);
    if (it == adc_channels_.end()) {
        return nullptr;
    }
    return it->second.get();
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

// Shared ADC unit handles - one per ADC unit
static std::map<adc_unit_t, adc_oneshot_unit_handle_t> shared_adc_handles;

// Real ADC implementation using new oneshot API with shared unit handles
class AdcChannelImpl : public IAdcChannel {
public:
    AdcChannelImpl(adc_unit_t unit, adc_channel_t channel, adc_atten_t attenuation, const std::string& id) 
        : unit_(unit), channel_(channel), attenuation_(attenuation), id_(id) {
        
        // Get or create shared ADC unit handle
        if (shared_adc_handles.find(unit) == shared_adc_handles.end()) {
            // Create new unit handle
            adc_oneshot_unit_init_cfg_t init_config = {
                .unit_id = unit,
                .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
                .ulp_mode = ADC_ULP_MODE_DISABLE,
            };
            
            adc_oneshot_unit_handle_t handle;
            esp_err_t ret = adc_oneshot_new_unit(&init_config, &handle);
            if (ret != ESP_OK) {
                ESP_LOGE("AdcChannelImpl", "Failed to initialize ADC unit %d: %s", unit, esp_err_to_name(ret));
                return;
            }
            
            shared_adc_handles[unit] = handle;
            ESP_LOGD("AdcChannelImpl", "Created new ADC unit handle for unit %d", unit);
        }
        
        // Configure this channel on the shared unit
        adc_oneshot_chan_cfg_t config = {
            .atten = attenuation,
            .bitwidth = ADC_BITWIDTH_12,
        };
        
        esp_err_t ret = adc_oneshot_config_channel(shared_adc_handles[unit], channel, &config);
        if (ret != ESP_OK) {
            ESP_LOGE("AdcChannelImpl", "Failed to configure ADC channel %d: %s", channel, esp_err_to_name(ret));
            return;
        }
        
        ESP_LOGD("AdcChannelImpl", "ADC channel %s configured: Unit=%d, Channel=%d, Atten=%d", 
                 id_.c_str(), unit, channel, attenuation);
    }
    
    HalResult<int> read_raw() override {
        HalResult<int> result;
        
        auto it = shared_adc_handles.find(unit_);
        if (it == shared_adc_handles.end()) {
            result.error = ESP_ERR_INVALID_STATE;
            result.value = 0;
            return result;
        }
        
        int adc_reading;
        esp_err_t err = adc_oneshot_read(it->second, channel_, &adc_reading);
        result.error = err;
        result.value = (err == ESP_OK) ? adc_reading : 0;
        
        return result;
    }
    
    HalResult<int> read_voltage_mv() override {
        auto raw = read_raw();
        if (raw.is_ok()) {
            HalResult<int> result;
            // Convert raw ADC reading to voltage in mV
            // This uses a simple linear conversion - for more accuracy, use esp_adc_cal
            result.value = (raw.value * 3300) / 4095;  // Assuming 3.3V reference and 12-bit ADC
            result.error = ESP_OK;
            return result;
        }
        return {0, raw.error};
    }
    
private:
    adc_unit_t unit_;
    adc_channel_t channel_;
    adc_atten_t attenuation_;
    std::string id_;
};

esp_err_t ESPhal::init_adc_channels() {
    ESP_LOGI(TAG, "Initializing ADC channels...");
    
    for (size_t i = 0; i < BoardConfig::ADC_CHANNELS_COUNT; i++) {
        const auto& config = BoardConfig::ADC_CHANNELS[i];
        
        ESP_LOGD(TAG, "  Creating ADC channel: %s on ADC%d_CH%d", 
                config.hal_id, config.unit, config.channel);
        
        // Create real ADC channel
        auto adc_channel = std::make_unique<AdcChannelImpl>(config.unit, config.channel, config.attenuation, config.hal_id);
        adc_channels_[config.hal_id] = std::move(adc_channel);
        
        ESP_LOGD(TAG, "    %s - Unit: %d, Channel: %d, Atten: %d", 
                config.hal_id, config.unit, config.channel, config.attenuation);
    }
    
    ESP_LOGI(TAG, "ADC channels initialized: %zu channels", BoardConfig::ADC_CHANNELS_COUNT);
    return ESP_OK;
}
