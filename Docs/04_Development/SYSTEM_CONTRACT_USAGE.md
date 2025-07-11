# System Contract - Developer Guide

## 🌡️ Temperature Control Example

If you're developing a climate control module and need temperature data:

### Option 1: Via SharedState (Recommended for current values)
```cpp
#include "generated_system_contract.h"

class ClimateControlModule : public IModule {
    void update() override {
        // Get current temperature
        float evap_temp = SharedState::get<float>(States::TEMP_EVAPORATOR);
        float ambient_temp = SharedState::get<float>(States::TEMP_AMBIENT);
        
        // Your control logic
        if (evap_temp < -25.0f) {
            // Too cold, adjust...
        }
    }
};
```

### Option 2: Via Events (For reacting to changes)
```cpp
void init() override {
    // Subscribe to temperature updates
    EventBus::subscribe(Events::SENSOR_READING_UPDATED, 
        [this](const EventBus::Event& event) {
            auto sensor_role = event.data["sensor_role"].get<std::string>();
            
            if (sensor_role == "temperature_evaporator") {
                float new_temp = event.data["value"];
                onEvaporatorTempChanged(new_temp);
            }
        });
}
```

## 🚪 Door State Example

For modules that need to know if door is open:

```cpp
// Check current state
bool door_open = SharedState::get<bool>(States::SENSOR_DOOR_OPEN);

// React to door events
EventBus::subscribe(Events::DOOR_STATE_CHANGED, [](const Event& e) {
    bool is_open = e.data["is_open"];
    ESP_LOGI(TAG, "Door is now %s", is_open ? "OPEN" : "CLOSED");
});
```

## 🔧 Publishing Module Status

Your module should publish its status:

```cpp
// Publish error
EventBus::publish(Events::CLIMATE_ERROR, {
    {"module", "climate_control"},
    {"error_code", ESP_ERR_TIMEOUT},
    {"message", "Compressor not responding"}
});

// Update state
SharedState::set(States::CLIMATE_MODE, "cooling");
SharedState::set(States::CLIMATE_SETPOINT, 4.0f);
```

## 📊 Complete Event/State Reference

See `generated_system_contract.md` for full list of:
- All available events with payload structure
- All SharedState keys with types and update rates
- Which module publishes what
- Expected value ranges
