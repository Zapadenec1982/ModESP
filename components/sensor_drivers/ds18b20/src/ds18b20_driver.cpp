/**
 * @file ds18b20_driver.cpp
 * @brief Implementation of DS18B20 temperature sensor driver
 */

#include "ds18b20_driver.h"
#include "sensor_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sstream>
#include <iomanip>

static const char* TAG = "DS18B20";

esp_err_t DS18B20Driver::init(ESPhal& hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing DS18B20 driver");
    
    // Parse configuration
    if (!config.contains("hal_id") || !config["hal_id"].is_string()) {
        ESP_LOGE(TAG, "Missing or invalid hal_id in configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!config.contains("address") || !config["address"].is_string()) {
        ESP_LOGE(TAG, "Missing or invalid address in configuration");
        return ESP_ERR_INVALID_ARG;
    }
    
    config_.hal_id = config["hal_id"].get<std::string>();
    config_.address = config["address"].get<std::string>();
    config_.resolution = config.value("resolution", 12);
    config_.offset = config.value("offset", 0.0f);
    config_.read_timeout_ms = config.value("read_timeout_ms", 1000);
    config_.use_crc = config.value("use_crc", true);
    
    // Validate configuration
    if (config_.resolution < 9 || config_.resolution > 12) {
        ESP_LOGE(TAG, "Invalid resolution: %d", config_.resolution);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get OneWire bus from HAL
    bus_ = hal.get_onewire_bus_ptr(config_.hal_id);
    if (!bus_) {
        ESP_LOGE(TAG, "Failed to get OneWire bus '%s'", config_.hal_id.c_str());
        return ESP_FAIL;
    }
    
    // Parse sensor address
    sensor_address_ = parse_address(config_.address);
    if (sensor_address_ == 0) {
        ESP_LOGE(TAG, "Invalid sensor address: %s", config_.address.c_str());
        return ESP_ERR_INVALID_ARG;
    }
    
    // Verify sensor presence
    auto devices = bus_->search_devices();
    sensor_available_ = false;
    
    for (auto addr : devices) {
        if (addr == sensor_address_) {
            sensor_available_ = true;
            ESP_LOGI(TAG, "Found sensor at address: 0x%016llx", sensor_address_);
            break;
        }
    }
    if (!sensor_available_) {
        ESP_LOGE(TAG, "Sensor not found on bus");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "DS18B20 initialized successfully");
    return ESP_OK;
}

SensorReading DS18B20Driver::read() {
    SensorReading reading;
    reading.unit = "°C";
    reading.timestamp_ms = esp_timer_get_time() / 1000;
    
    if (!sensor_available_) {
        reading.is_valid = false;
        reading.error_message = "Sensor not available";
        return reading;
    }
    
    // Request temperature conversion
    esp_err_t ret = bus_->request_temperatures();
    if (ret != ESP_OK) {
        error_count_++;
        reading.is_valid = false;
        reading.error_message = "Failed to request temperature";
        return reading;
    }
    
    // Wait for conversion
    vTaskDelay(pdMS_TO_TICKS(get_conversion_time_ms()));
    
    // Read temperature
    auto result = bus_->read_temperature(sensor_address_);
    if (!result.is_ok()) {
        error_count_++;
        reading.is_valid = false;
        reading.error_message = "Failed to read temperature";
        return reading;
    }
    
    float temperature = result.value;
    
    // Validate temperature range
    if (!validate_temperature(temperature)) {
        error_count_++;
        reading.is_valid = false;
        reading.error_message = "Temperature out of range";
        return reading;
    }
    
    // Apply calibration offset
    temperature += config_.offset;
    
    // Update state
    last_temperature_ = temperature;
    successful_reads_++;
    
    reading.value = temperature;
    reading.is_valid = true;
    
    return reading;
}

nlohmann::json DS18B20Driver::get_config() const {
    return {
        {"hal_id", config_.hal_id},
        {"address", config_.address},
        {"resolution", config_.resolution},
        {"offset", config_.offset},
        {"read_timeout_ms", config_.read_timeout_ms},
        {"use_crc", config_.use_crc}
    };
}

esp_err_t DS18B20Driver::set_config(const nlohmann::json& config) {
    if (!config.is_object()) {
        ESP_LOGE(TAG, "Configuration must be an object");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config.contains("resolution") && config["resolution"].is_number()) {
        int res = config["resolution"].get<int>();
        if (res >= 9 && res <= 12) {
            config_.resolution = res;
        }
    }
    
    if (config.contains("offset") && config["offset"].is_number()) {
        config_.offset = config["offset"].get<float>();
    }
    
    if (config.contains("use_crc") && config["use_crc"].is_boolean()) {
        config_.use_crc = config["use_crc"].get<bool>();
    }
    
    return ESP_OK;
}

nlohmann::json DS18B20Driver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "DS18B20 Temperature Sensor Settings"},
        {"properties", {
            {"resolution", {
                {"type", "integer"},
                {"title", "Resolution"},
                {"minimum", 9},
                {"maximum", 12},
                {"default", 12}
            }},
            {"offset", {
                {"type", "number"},
                {"title", "Temperature Offset"},
                {"minimum", -10.0},
                {"maximum", 10.0},
                {"default", 0.0}
            }},
            {"use_crc", {
                {"type", "boolean"},
                {"title", "Enable CRC Check"},
                {"default", true}
            }}
        }}
    };
}

esp_err_t DS18B20Driver::calibrate(const nlohmann::json& calibration_data) {
    if (!calibration_data.is_object()) {
        ESP_LOGE(TAG, "Calibration data must be an object");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (calibration_data.contains("reference_temp") && 
        calibration_data.contains("measured_temp")) {
        
        float ref_temp = calibration_data["reference_temp"].get<float>();
        float measured_temp = calibration_data["measured_temp"].get<float>();
        
        // Calculate offset correction
        config_.offset = ref_temp - measured_temp;
        
        ESP_LOGI(TAG, "Calibration applied: offset = %.2f°C", config_.offset);
        return ESP_OK;
    }
    
    return ESP_ERR_INVALID_ARG;
}

nlohmann::json DS18B20Driver::get_diagnostics() const {
    return {
        {"driver_type", "DS18B20"},
        {"sensor_address", config_.address},
        {"last_temperature", last_temperature_},
        {"successful_reads", successful_reads_},
        {"error_count", error_count_},
        {"sensor_available", sensor_available_},
        {"resolution_bits", config_.resolution}
    };
}

// Helper methods
int DS18B20Driver::get_conversion_time_ms() const {
    // DS18B20 conversion times by resolution:
    // 9 bits: 93.75ms, 10 bits: 187.5ms, 11 bits: 375ms, 12 bits: 750ms
    switch (config_.resolution) {
        case 9: return 100;
        case 10: return 200;
        case 11: return 400;
        case 12: return 750;
        default: return 750;
    }
}

bool DS18B20Driver::validate_temperature(float temp) const {
    // DS18B20 valid range: -55°C to +125°C
    return (temp >= -55.0f && temp <= 125.0f);
}

uint64_t DS18B20Driver::parse_address(const std::string& address_str) {
    if (address_str.empty()) {
        return 0;
    }
    
    // Remove any "0x" prefix
    std::string clean_addr = address_str;
    if (clean_addr.substr(0, 2) == "0x" || clean_addr.substr(0, 2) == "0X") {
        clean_addr = clean_addr.substr(2);
    }
    
    // Parse hex string
    uint64_t address = 0;
    if (clean_addr.length() != 16) {
        ESP_LOGE(TAG, "Address string must be exactly 16 hex characters");
        return 0;
    }
    
    for (char c : clean_addr) {
        address <<= 4;
        if (c >= '0' && c <= '9') {
            address |= (c - '0');
        } else if (c >= 'A' && c <= 'F') {
            address |= (c - 'A' + 10);
        } else if (c >= 'a' && c <= 'f') {
            address |= (c - 'a' + 10);
        } else {
            ESP_LOGE(TAG, "Invalid hex character in address: %c", c);
            return 0;
        }
    }
    
    return address;
}