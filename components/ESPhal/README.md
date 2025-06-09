# HAL Architecture - Corrected Implementation

## Overview

The ModuChill system uses a HAL (Hardware Abstraction Layer) architecture that follows the documentation exactly. All sensor drivers are **internal parts of SensorModule**, not separate components.

## Correct Architecture Structure

```
components/
├── ESPhal/                          # Core HAL framework
│   ├── modules/                     # HAL modules (hardware interfaces)
│   │   ├── relay_module/            # Controls relay outputs
│   │   └── sensor_module/           # Contains ALL sensor drivers internally
│   │       ├── include/
│   │       │   └── sensor_module.h  # Contains DS18B20Driver, NTCDriver, etc.
│   │       └── src/
│   │           └── sensor_module.cpp # All drivers implemented here
│   ├── include/                     # HAL interfaces (IOneWireBus, etc.)
│   └── src/                         # HAL core (provides buses/controllers)
└── core/configs/
    ├── sensors.json                 # Sensor configuration
    └── relays.json                  # Relay configuration
```

## Key Principles (Following Documentation)

### 1. **SensorModule Contains All Drivers**
From documentation:
> "SensorModule створює внутрішній об'єкт, що представляє датчик... Зберігає готовий об'єкт датчика у внутрішньому списку."

```cpp
class SensorModule : public BaseModule {
private:
    std::vector<std::unique_ptr<ISensorDriver>> sensors_;  // Internal sensor objects
    
    // Internal driver classes
    class DS18B20Driver : public ISensorDriver { /* ... */ };
    class NTCDriver : public ISensorDriver { /* ... */ };
    class GpioInputDriver : public ISensorDriver { /* ... */ };
};
```

### 2. **HAL Provides Resources, Not Drivers**
From HAL documentation:
> "HAL надає методи-фабрики для отримання доступу до ресурсів"

```cpp
// SensorModule gets resources from HAL
IOneWireBus& bus = hal_.get_onewire_bus("ONEWIRE_CHAMBER");
IAdcChannel& adc = hal_.get_adc_channel("ADC_AMBIENT_TEMP");

// SensorModule creates internal drivers using these resources
auto driver = std::make_unique<DS18B20Driver>(bus, address);
```

### 3. **Configuration-Based Modularity**
Modularity is achieved through:

**JSON Configuration:**
```json
{
  "sensors": [
    {
      "role": "chamber_temp",
      "type": "DS18B20",           // SensorModule knows how to create this
      "hal_id": "ONEWIRE_CHAMBER", // HAL resource identifier
      "address": "28ff640264013c28"
    }
  ]
}
```

**Compile-time Options:**
```cmake
# In menuconfig
CONFIG_ENABLE_DS18B20_SENSORS=y
CONFIG_ENABLE_NTC_SENSORS=n
```

## Pattern Implementation

### 1. **SensorModule Initialization (Following Documentation)**
```cpp
esp_err_t SensorModule::init() {
    // Read configuration
    // For each sensor config:
    //   1. Get HAL resource by hal_id
    //   2. Create internal sensor object
    //   3. Store in internal list
    
    for (const auto& config : sensor_configs_) {
        IOneWireBus& bus = hal_.get_onewire_bus(config.hal_id);
        auto sensor = std::make_unique<DS18B20Driver>(bus, config.address);
        sensors_.push_back(std::move(sensor));  // Internal storage
    }
}
```

### 2. **Update Pattern (Following Documentation)**
```cpp
void SensorModule::update() {
    // Poll all internal sensors
    // Publish standardized data to SharedState
    
    for (size_t i = 0; i < sensors_.size(); i++) {
        SensorReading reading = sensors_[i]->read();
        SharedState::set(sensor_configs_[i].publish_key, reading.to_json());
    }
}
```

## Benefits of Correct Architecture

✅ **Follows Documentation** - Exact implementation as specified  
✅ **Single Responsibility** - SensorModule owns all sensor logic  
✅ **HAL Simplicity** - HAL only provides hardware resources  
✅ **Configuration Modularity** - Enable/disable through JSON + Kconfig  
✅ **Maintainability** - All sensor code in one place  

## Adding New Sensor Types

### Step 1: Add Internal Driver Class
```cpp
// In sensor_module.h
class NewSensorDriver : public ISensorDriver {
public:
    NewSensorDriver(IAdcChannel& adc, const nlohmann::json& config);
    SensorReading read() override;
    // ... other methods
};
```

### Step 2: Add to Factory Method
```cpp
// In sensor_module.cpp
std::unique_ptr<ISensorDriver> SensorModule::create_sensor_driver(const SensorConfig& config) {
    switch (config.type) {
        case SensorType::NEW_SENSOR:
            return std::make_unique<NewSensorDriver>(hal_.get_adc_channel(config.hal_id), config);
        // ... other cases
    }
}
```

### Step 3: Add Configuration Support
```json
{
  "sensors": [
    {
      "role": "new_measurement", 
      "type": "NEW_SENSOR",
      "hal_id": "ADC_CHANNEL_X",
      "custom_param": "value"
    }
  ]
}
```

## Key Difference from Wrong Implementation

❌ **Wrong:** Separate sensor driver components  
✅ **Right:** Internal sensor drivers in SensorModule  

❌ **Wrong:** Driver registry pattern  
✅ **Right:** Direct driver creation in SensorModule  

❌ **Wrong:** ESP-IDF component modularity  
✅ **Right:** Configuration + compile-time modularity  

This implementation now correctly follows the documentation!
