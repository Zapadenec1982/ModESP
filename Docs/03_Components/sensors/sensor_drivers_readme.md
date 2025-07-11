# Sensor Drivers Component

This component provides modular sensor driver architecture for the ModESP system.

## Architecture

The sensor drivers follow a plugin-based architecture where each driver:
- Implements the `ISensorDriver` interface
- Is self-contained with all sensor-specific logic
- Registers itself with the `SensorDriverRegistry`
- Provides UI schema for configuration

## Currently Implemented Drivers

- **DS18B20AsyncDriver** - Non-blocking driver for DS18B20 OneWire temperature sensors
- **NTCDriver** - Driver for NTC thermistors with various profiles
- **GpioInputDriver** - Driver for digital inputs (switches, buttons)

## Driver Interface

Each driver must implement the following methods:

```cpp
class ISensorDriver {
public:
    // Initialize with HAL and config
    virtual esp_err_t init(ESPhal* hal, const nlohmann::json& config) = 0;
    
    // Read sensor value
    virtual SensorReading read() = 0;
    
    // Get driver type identifier
    virtual std::string get_type() const = 0;
    
    // Configuration management
    virtual nlohmann::json get_config() const = 0;
    virtual esp_err_t set_config(const nlohmann::json& config) = 0;
    
    // UI schema for configuration
    virtual nlohmann::json get_ui_schema() const = 0;
    
    // Optional calibration
    virtual esp_err_t calibrate(const nlohmann::json& calibration_data);
};
```

## Adding a New Driver

### 1. Create driver header
```cpp
// my_sensor_driver.h
#pragma once
#include "sensor_driver_interface.h"

class MySensorDriver : public ISensorDriver {
    // Implementation
};
```

### 2. Implement driver
```cpp
// my_sensor_driver.cpp
#include "my_sensor_driver.h"
#include "sensor_driver_registry.h"

// Implementation...

// Auto-register driver
static bool registered = []() {
    SensorDriverRegistry::instance().register_driver(
        "MY_SENSOR",
        []() -> std::unique_ptr<ISensorDriver> {
            return std::make_unique<MySensorDriver>();
        }
    );
    return true;
}();
```

### 3. Add to build system
Update `CMakeLists.txt` and optionally add Kconfig options.

## Configuration

Drivers are configured through JSON:

```json
{
  "sensors": [
    {
      "role": "temperature",
      "type": "DS18B20_Async",
      "config": {
        "hal_id": "ONEWIRE_BUS_1",
        "address": "28FF123456789012",
        "resolution": 12
      }
    }
  ]
}
```

## Memory Optimization

Use Kconfig to disable unused drivers and save flash space:

```bash
idf.py menuconfig
# Component config → Sensor Drivers Configuration
```
