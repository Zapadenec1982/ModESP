/**
 * @file pressure_4_20ma_driver.h
 * @brief Industrial 4-20mA pressure sensor driver
 * 
 * Self-contained driver for industrial pressure sensors with 4-20mA current loop output.
 * Requires proper signal conditioning circuit to convert current to voltage.
 */

#pragma once

#include "sensor_driver_interface.h"
#include "hal_interfaces.h"

/**
 * @brief 4-20mA pressure sensor driver
 * 
 * Features:
 * - Configurable pressure range (min/max)
 * - Multiple units support (bar, PSI, kPa)
 * - Automatic fault detection (< 4mA or > 20mA)
 * - Zero and span calibration
 * - Moving average filter
 * - Out-of-range alarms
 */
class Pressure420mADriver : public ISensorDriver {
public:
    enum class PressureUnit {
        BAR,
        PSI,
        KPA,
        MPA
    };
    
    Pressure420mADriver() = default;
    ~Pressure420mADriver() override = default;    
    // ISensorDriver interface implementation
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    SensorReading read() override;
    std::string get_type() const override { return "PRESSURE_4_20MA"; }
    std::string get_description() const override { return "4-20mA Pressure Sensor"; }
    bool is_available() const override { return adc_channel_ != nullptr; }
    nlohmann::json get_config() const override;
    esp_err_t set_config(const nlohmann::json& config) override;
    nlohmann::json get_ui_schema() const override;
    esp_err_t calibrate(const nlohmann::json& calibration_data) override;
    nlohmann::json get_diagnostics() const override;

private:
    // Configuration parameters
    struct Config {
        std::string hal_id;             // ADC channel HAL identifier
        float pressure_min = 0.0f;      // Pressure at 4mA
        float pressure_max = 10.0f;     // Pressure at 20mA
        PressureUnit unit = PressureUnit::BAR;
        float r_sense = 250.0f;         // Current sense resistor (ohms)
        float vcc = 3.3f;               // ADC reference voltage
        int averaging_samples = 10;      // Moving average samples
        float zero_offset = 0.0f;       // Zero calibration offset
        float span_factor = 1.0f;       // Span calibration factor
        
        // Alarm thresholds
        float alarm_low = -1.0f;        // Low pressure alarm (-1 = disabled)
        float alarm_high = -1.0f;       // High pressure alarm (-1 = disabled)
        float fault_threshold_low = 3.5f;  // Current below this = sensor fault
        float fault_threshold_high = 20.5f; // Current above this = sensor fault
    } config_;    
    // Runtime state
    IAdcChannel* adc_channel_ = nullptr;
    std::vector<float> sample_buffer_;
    size_t buffer_index_ = 0;
    bool buffer_filled_ = false;
    
    uint32_t total_reads_ = 0;
    uint32_t fault_count_ = 0;
    uint32_t alarm_low_count_ = 0;
    uint32_t alarm_high_count_ = 0;
    
    float last_pressure_ = 0.0f;
    float last_current_ma_ = 0.0f;
    bool last_fault_state_ = false;
    
    // Helper methods
    float adc_to_current_ma(int adc_value) const;
    float current_to_pressure(float current_ma) const;
    std::string unit_to_string(PressureUnit unit) const;
    float convert_pressure(float pressure, PressureUnit from, PressureUnit to) const;
    void update_moving_average(float value);
    float get_average_pressure() const;
};