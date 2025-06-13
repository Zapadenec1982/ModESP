# Приклад використання RTCModule в інших модулях

## SensorModule - додавання timestamps до вимірювань

```cpp
// В sensor_module.cpp
#include "rtc_module.h"

void SensorModule::publish_sensor_data(const SensorInstance& sensor, 
                                      const ::SensorReading& reading) {
    // Додаємо timestamp до даних датчика
    nlohmann::json sensor_data = reading.to_json();
    sensor_data["timestamp"] = RTCModule::get_timestamp();
    sensor_data["time_string"] = RTCModule::get_time_string();
    
    SharedState::set(sensor.config.publish_key, sensor_data);
}
```

## ActuatorModule - відстеження часу роботи

```cpp
// В relay_driver.cpp
#include "rtc_module.h"

void RelayDriver::apply_state(bool state) {
    if (state != current_state_) {
        current_state_ = state;
        
        // Зберігаємо точний час зміни стану
        last_change_time_ms_ = RTCModule::get_timestamp() * 1000;
        
        // Логування з timestamp
        ESP_LOGI(TAG, "[%s] Relay %s: %s", 
                 RTCModule::get_time_string().c_str(),
                 config_.hal_id.c_str(),
                 state ? "ON" : "OFF");
    }
}
```

## ClimateControl - логування з часом

```cpp
// В climate_control.cpp (майбутній модуль)
#include "rtc_module.h"

void ClimateControl::log_temperature_event(float temp, const std::string& event) {
    // Створюємо запис для логування
    nlohmann::json log_entry = {
        {"timestamp", RTCModule::get_timestamp()},
        {"time_string", RTCModule::get_time_string()},
        {"temperature", temp},
        {"event", event},
        {"setpoint", config_.setpoint},
        {"uptime", RTCModule::get_uptime_seconds()}
    };
    
    // Публікуємо в EventBus
    EventBus::publish("climate.temperature.event", log_entry);
    
    // Логування
    ESP_LOGI(TAG, "[%s] %s: temp=%.1f°C, setpoint=%.1f°C",
             RTCModule::get_time_string().c_str(),
             event.c_str(), temp, config_.setpoint);
}
```

## Приклад даних в SharedState з timestamps

```json
{
  "state.sensor.chamber_temp": {
    "value": 4.2,
    "unit": "°C",
    "is_valid": true,
    "timestamp": 1704067200,
    "time_string": "2024-01-01 12:00:00"
  },
  "state.actuator.compressor": {
    "is_active": true,
    "state": "RUNNING",
    "last_change_ms": 1704067200000,
    "time_string": "2024-01-01 12:00:00"
  },
  "state.time": {
    "timestamp": 1704067260,
    "time_string": "2024-01-01 12:01:00",
    "uptime_seconds": 3660,
    "timezone": "UTC",
    "is_valid": true
  }
}
```

## Використання для HACCP логування

```cpp
// Періодичне логування температури для HACCP
void log_haccp_temperature() {
    auto temp_data = SharedState::get("state.sensor.chamber_temp");
    
    if (temp_data.contains("value")) {
        float temperature = temp_data["value"];
        
        // Формуємо HACCP запис
        std::string haccp_log = fmt::format(
            "HACCP,{},{:.1f},{}",
            RTCModule::get_timestamp(),
            temperature,
            RTCModule::get_time_string("%Y-%m-%d %H:%M:%S")
        );
        
        // Зберігаємо в persistent storage (майбутній StorageModule)
        // StorageModule::log_haccp(haccp_log);
        
        ESP_LOGI("HACCP", "%s", haccp_log.c_str());
    }
}
```
