/**
 * @file ntc_driver.h
 * @brief NTC thermistor temperature sensor driver
 * 
 * Self-contained driver for NTC thermistors with ADC interface.
 * Supports various NTC types with Steinhart-Hart equation.
 */

#pragma once

#include "sensor_driver_interface.h"
#include "hal_interfaces.h"
#include <array>

/**
 * @brief NTC thermistor sensor driver
 * 
 * Features:
 * - Multiple NTC curve profiles (10K, 100K, etc.)
 * - Steinhart-Hart equation for accurate conversion
 * - Beta coefficient calculation
 * - Automatic averaging for noise reduction
 * - Configurable voltage divider parameters
 */
class NTCDriver : public ISensorDriver {
public:
    // Common NTC types
    enum class NTCType {
        NTC_10K_3950,   // 10K @ 25°C, Beta = 3950
        NTC_10K_3435,   // 10K @ 25°C, Beta = 3435
        NTC_100K_3950,  // 100K @ 25°C, Beta = 3950
        CUSTOM          // User-defined parameters
    };
    
    NTCDriver() = default;
    ~NTCDriver() override = default;    
    // ISensorDriver interface implementation
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    SensorReading read() override;
    std::string get_type() const override { return "NTC"; }
    std::string get_description() const override { return "NTC Thermistor Temperature Sensor"; }
    bool is_available() const override { return adc_channel_ != nullptr; }
    nlohmann::json get_config() const override;
    esp_err_t set_config(const nlohmann::json& config) override;
    nlohmann::json get_ui_schema() const override;
    esp_err_t calibrate(const nlohmann::json& calibration_data) override;
    nlohmann::json get_diagnostics() const override;

private:
    // Configuration parameters
    struct Config {
        std::string hal_id;          // ADC channel HAL identifier
        NTCType ntc_type = NTCType::NTC_10K_3950;
        float r_nominal = 10000.0f;  // Nominal resistance at T_nominal
        float t_nominal = 25.0f;     // Nominal temperature (°C)
        float beta = 3950.0f;        // Beta coefficient
        float r_series = 10000.0f;   // Series resistor value
        float vcc = 3.3f;            // Supply voltage
        int averaging_samples = 10;   // Number of samples to average
        float offset = 0.0f;         // Temperature offset for calibration
        
        // Steinhart-Hart coefficients (optional, for better accuracy)
        std::array<float, 3> sh_coefficients = {0, 0, 0};
        bool use_steinhart_hart = false;
    } config_;
    
    // Runtime state
    IAdcChannel* adc_channel_ = nullptr;
    std::vector<float> sample_buffer_;
    uint32_t total_reads_ = 0;
    uint32_t error_count_ = 0;
    float last_temperature_ = 0.0f;
    float last_resistance_ = 0.0f;
    
    // Helper methods
    float calculate_resistance(int adc_value) const;
    float resistance_to_temperature(float resistance) const;
    float steinhart_hart(float resistance) const;
    void load_ntc_profile(NTCType type);
};