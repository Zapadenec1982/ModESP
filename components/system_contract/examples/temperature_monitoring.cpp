/**
 * @file temperature_monitoring.cpp
 * @brief Example: Monitoring temperature sensors
 */

#include "generated_system_contract.h"
#include "event_bus.h"
#include "shared_state.h"

class TemperatureMonitor {
public:
    void init() {
        // Subscribe to temperature updates
        EventBus::subscribe(Events::SENSOR_READING_UPDATED, 
            [this](const EventBus::Event& event) {
                auto sensor_role = event.data["sensor_role"].get<std::string>();
                
                if (sensor_role == "temperature_evaporator") {
                    float temp = event.data["value"];
                    onEvaporatorTempChanged(temp);
                }
            });
    }
    
    void checkCurrentTemperature() {
        // Read current temperature from SharedState
        float evap_temp = SharedState::get<float>(States::TEMP_EVAPORATOR);
        float ambient_temp = SharedState::get<float>(States::TEMP_AMBIENT);
        
        ESP_LOGI(TAG, "Evaporator: %.1f°C, Ambient: %.1f°C", 
                 evap_temp, ambient_temp);
    }
    
private:
    void onEvaporatorTempChanged(float temp) {
        if (temp < -30.0f) {
            // Publish warning event
            EventBus::publish(Events::SYSTEM_HEALTH_WARNING, {
                {"module", "temperature_monitor"},
                {"reason", "Evaporator too cold"},
                {"value", temp}
            });
        }
    }
    
    static constexpr const char* TAG = "TempMonitor";
};
