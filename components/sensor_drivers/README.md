# Модульна архітектура драйверів датчиків

## Огляд

Система ModuChill використовує повністю модульну архітектуру для драйверів датчиків, де кожен тип датчика є окремим самодостатнім компонентом. Це дозволяє:

- ✅ Додавати нові типи датчиків без зміни коду SensorModule
- ✅ Видаляти непотрібні драйвери через конфігурацію
- ✅ Кожен драйвер містить всю специфічну логіку, налаштування та UI схему
- ✅ SensorModule працює з уніфікованим інтерфейсом, не знаючи деталей датчиків

## Архітектура

```
┌─────────────────┐     ┌──────────────────┐
│  SensorModule   │────▶│ ISensorDriver    │ (interface)
└─────────────────┘     └──────────────────┘
         │                       ▲
         │                       │ implements
         ▼                       │
┌─────────────────┐     ┌──────────────────┐
│ DriverRegistry  │────▶│  DS18B20Driver   │
└─────────────────┘     ├──────────────────┤
                        │  NTCDriver       │
                        ├──────────────────┤
                        │  PressureDriver  │
                        └──────────────────┘
```

## Структура файлів

```
components/
├── sensor_drivers/              # Базові інтерфейси та реєстр
│   ├── include/
│   │   ├── sensor_driver_interface.h
│   │   └── sensor_driver_registry.h
│   ├── CMakeLists.txt
│   └── Kconfig
├── sensor_drivers/ds18b20/      # Драйвер DS18B20
│   ├── include/
│   │   └── ds18b20_driver.h
│   ├── src/
│   │   └── ds18b20_driver.cpp
│   └── CMakeLists.txt
├── sensor_drivers/ntc/          # Драйвер NTC термістора
│   ├── include/
│   │   └── ntc_driver.h
│   ├── src/
│   │   └── ntc_driver.cpp
│   └── CMakeLists.txt
└── sensor_drivers/pressure/     # Драйвер датчика тиску (приклад)
    ├── include/
    │   └── pressure_driver.h
    ├── src/
    │   └── pressure_driver.cpp
    └── CMakeLists.txt
```

## Додавання нового драйвера

### 1. Створіть новий компонент драйвера

```bash
mkdir -p components/sensor_drivers/my_sensor/{include,src}
```

### 2. Реалізуйте інтерфейс ISensorDriver

```cpp
// my_sensor_driver.h
#pragma once
#include "sensor_driver_interface.h"

class MySensorDriver : public ISensorDriver {
public:
    // Реалізуйте всі методи інтерфейсу
    esp_err_t init(ESPhal& hal, const nlohmann::json& config) override;
    SensorReading read() override;
    std::string get_type() const override { return "MY_SENSOR"; }
    // ... інші методи
};
```

### 3. Зареєструйте драйвер

```cpp
// my_sensor_driver.cpp
#include "my_sensor_driver.h"
#include "sensor_driver_registry.h"

// Автоматична реєстрація при старті
static SensorDriverRegistrar<MySensorDriver> registrar("MY_SENSOR");

// Реалізація методів...
```

### 4. Створіть CMakeLists.txt

```cmake
idf_component_register(
    SRCS "src/my_sensor_driver.cpp"
    INCLUDE_DIRS "include"
    REQUIRES sensor_drivers ESPhal json
)
```

### 5. Додайте в Kconfig (опційно)

```kconfig
config ENABLE_MY_SENSOR_DRIVER
    bool "Enable My Sensor driver"
    default n
    help
        Enable support for My Sensor devices.
```

## Конфігурація

### sensors.json

```json
{
  "sensors": [
    {
      "role": "my_measurement",
      "type": "MY_SENSOR",              // Тип драйвера (як зареєстровано)
      "publish_key": "state.sensor.my_measurement",
      "config": {                       // Специфічна конфігурація драйвера
        "hal_id": "ADC_CHANNEL_X",
        "custom_param": 42,
        "calibration": [1.0, 0.0]
      }
    }
  ]
}
```

## UI схема драйвера

Кожен драйвер повертає JSON схему для автоматичної генерації UI:

```cpp
nlohmann::json MySensorDriver::get_ui_schema() const {
    return {
        {"type", "object"},
        {"title", "My Sensor Settings"},
        {"properties", {
            {"sensitivity", {
                {"type", "number"},
                {"title", "Sensitivity"},
                {"minimum", 0.1},
                {"maximum", 10.0},
                {"default", 1.0},
                {"ui:widget", "slider"}
            }},
            {"mode", {
                {"type", "string"},
                {"title", "Operating Mode"},
                {"enum", {"normal", "fast", "precise"}},
                {"default", "normal"},
                {"ui:widget", "select"}
            }}
        }}
    };
}
```

## Переваги архітектури

1. **Модульність**: Кожен драйвер - окремий компонент
2. **Розширюваність**: Додавання нових типів без зміни ядра
3. **Інкапсуляція**: Вся логіка датчика в одному місці
4. **Конфігурованість**: Вибір драйверів через menuconfig
5. **UI інтеграція**: Автоматична генерація налаштувань
6. **Тестованість**: Кожен драйвер тестується окремо

## Приклади використання

### SensorModule не знає про типи датчиків

```cpp
// SensorModule просто створює драйвери через реєстр
auto driver = SensorDriverRegistry::instance().create_driver(config.type);
if (driver) {
    driver->init(hal_, config.config);
    sensors_.push_back({std::move(driver), config});
}
```

### Динамічне отримання доступних драйверів

```cpp
auto available = SensorDriverRegistry::instance().get_registered_types();
// ["DS18B20", "NTC", "PRESSURE_4_20MA", ...]
```

Ця архітектура забезпечує максимальну гнучкість та розширюваність системи!