/**
 * @file ds18b20_async_driver.cpp
 * @brief Implementation of asynchronous DS18B20 temperature sensor driver
 */

#include "ds18b20_async_driver.h"
#include "sensor_driver_registry.h"
#include "esphal.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <sstream>
#include <iomanip>

static const char* TAG = "DS18B20_Async";

esp_err_t DS18B20AsyncDriver::init(ESPhal* hal, const nlohmann::json& config) {
    ESP_LOGI(TAG, "Initializing DS18B20 async driver");
    
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
    config_.max_retries = config.value("max_retries", 3);
    
    // Validate configuration
    if (config_.resolution < 9 || config_.resolution > 12) {
        ESP_LOGE(TAG, "Invalid resolution: %d", config_.resolution);
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get OneWire bus from HAL
    bus_ = hal->get_onewire_bus_ptr(config_.hal_id);
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
    
    // Initialize state
    state_ = State::IDLE;
    has_valid_reading_ = false;
    
    ESP_LOGI(TAG, "DS18B20 async driver initialized successfully");
    return ESP_OK;
}

SensorReading DS18B20AsyncDriver::read() {
    SensorReading reading;
    reading.unit = "°C";
    reading.timestamp_ms = esp_timer_get_time() / 1000;
    
    if (!sensor_available_) {
        reading.is_valid = false;
        reading.error_message = "Sensor not available";
        return reading;
    }    
    int64_t current_time_ms = esp_timer_get_time() / 1000;
    
    switch (state_) {
        case State::IDLE:
            // Start new conversion
            ESP_LOGV(TAG, "Starting temperature conversion");
            if (bus_->request_temperatures() == ESP_OK) {
                state_ = State::CONVERSION_REQUESTED;
                conversion_start_time_ms_ = current_time_ms;
                total_conversions_++;
                retry_count_ = 0;
            } else {
                ESP_LOGE(TAG, "Failed to request temperature");
                error_count_++;
                reading.is_valid = false;
                reading.error_message = "Failed to request temperature";
            }
            break;
            
        case State::CONVERSION_REQUESTED:
            // Move to waiting state
            state_ = State::WAITING_FOR_CONVERSION;
            break;
            
        case State::WAITING_FOR_CONVERSION:
            // Check if conversion time has elapsed
            if (current_time_ms - conversion_start_time_ms_ >= get_conversion_time_ms()) {
                state_ = State::READY_TO_READ;
            }
            break;
            
        case State::READY_TO_READ:
            // Try to read temperature
            {
                auto result = bus_->read_temperature(sensor_address_);
                if (result.is_ok()) {
                    float temperature = result.value;
                    
                    // Validate temperature
                    if (validate_temperature(temperature)) {
                        // Apply calibration offset
                        temperature += config_.offset;
                        
                        // Update cached values
                        last_temperature_ = temperature;
                        last_valid_read_time_ms_ = current_time_ms;
                        has_valid_reading_ = true;
                        successful_reads_++;
                        
                        // Return valid reading
                        reading.value = temperature;
                        reading.is_valid = true;
                        
                        // Reset to idle for next conversion
                        state_ = State::IDLE;
                    } else {
                        // Temperature out of range
                        ESP_LOGW(TAG, "Temperature out of range: %.2f", temperature);
                        error_count_++;
                        
                        if (++retry_count_ < config_.max_retries) {
                            // Retry conversion
                            state_ = State::IDLE;
                        } else {
                            // Max retries reached
                            state_ = State::ERROR;
                        }
                    }
                } else {
                    // Read failed
                    ESP_LOGW(TAG, "Failed to read temperature");
                    error_count_++;
                    
                    if (++retry_count_ < config_.max_retries) {
                        // Retry conversion
                        state_ = State::IDLE;
                    } else {
                        // Max retries reached
                        state_ = State::ERROR;
                    }
                }
            }
            break;
            
        case State::ERROR:
            // Reset after error
            reset_state();
            reading.is_valid = false;
            reading.error_message = "Max retries exceeded";
            break;
    }
    
    // If we don't have a valid reading yet, return cached value or error
    if (!reading.is_valid && has_valid_reading_) {
        // Return cached value if it's not too old
        if (current_time_ms - last_valid_read_time_ms_ < 60000) { // 1 minute
            reading.value = last_temperature_;
            reading.is_valid = true;
            reading.error_message = ""; // Clear any error message
        } else {
            reading.is_valid = false;
            reading.error_message = "Stale data";
        }
    } else if (!reading.is_valid) {
        reading.error_message = "No valid reading available";
    }
    
    return reading;
}

void DS18B20AsyncDriver::reset_state() {
    state_ = State::IDLE;
    retry_count_ = 0;
}

nlohmann::json DS18B20AsyncDriver::get_config() const {
    return {
        {"hal_id", config_.hal_id},
        {"address", config_.address},
        {"resolution", config_.resolution},
        {"offset", config_.offset},
        {"read_timeout_ms", config_.read_timeout_ms},
        {"use_crc", config_.use_crc},
        {"max_retries", config_.max_retries}
    };
}

esp_err_t DS18B20AsyncDriver::set_config(const nlohmann::json& config) {
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
    
    if (config.contains("max_retries") && config["max_retries"].is_number()) {
        config_.max_retries = config["max_retries"].get<int>();
    }
    
    return ESP_OK;
}

nlohmann::json DS18B20AsyncDriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "DS18B20 Async Temperature Sensor Settings"},
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
            }},
            {"max_retries", {
                {"type", "integer"},
                {"title", "Max Retries"},
                {"minimum", 1},
                {"maximum", 10},
                {"default", 3}
            }}
        }}
    };
}

esp_err_t DS18B20AsyncDriver::calibrate(const nlohmann::json& calibration_data) {
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

nlohmann::json DS18B20AsyncDriver::get_diagnostics() const {
    std::string state_str;
    switch (state_) {
        case State::IDLE: state_str = "IDLE"; break;
        case State::CONVERSION_REQUESTED: state_str = "CONVERSION_REQUESTED"; break;
        case State::WAITING_FOR_CONVERSION: state_str = "WAITING_FOR_CONVERSION"; break;
        case State::READY_TO_READ: state_str = "READY_TO_READ"; break;
        case State::ERROR: state_str = "ERROR"; break;
    }
    
    return {
        {"driver_type", "DS18B20_Async"},
        {"sensor_address", config_.address},
        {"current_state", state_str},
        {"has_valid_reading", has_valid_reading_},
        {"last_temperature", last_temperature_},
        {"successful_reads", successful_reads_},
        {"error_count", error_count_},
        {"total_conversions", total_conversions_},
        {"sensor_available", sensor_available_},
        {"resolution_bits", config_.resolution},
        {"retry_count", retry_count_}
    };
}

// Helper methods
int DS18B20AsyncDriver::get_conversion_time_ms() const {
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

bool DS18B20AsyncDriver::validate_temperature(float temp) const {
    // DS18B20 valid range: -55°C to +125°C
    return (temp >= -55.0f && temp <= 125.0f);
}

uint64_t DS18B20AsyncDriver::parse_address(const std::string& address_str) {
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

// Register driver with registry
static bool registered = []() {
    SensorDriverRegistry::instance().register_driver(
        "DS18B20_Async",
        []() -> std::unique_ptr<ISensorDriver> {
            return std::make_unique<DS18B20AsyncDriver>();
        }
    );
    return true;
}();
