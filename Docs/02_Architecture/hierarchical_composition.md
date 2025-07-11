# Hierarchical Module Composition

## Концепція
Модулі-менеджери (SensorManager, ClimateManager) агрегують функціональність драйверів/контролерів та надають єдиний інтерфейс для системи.

## Структура

### 1. Driver Manifest
Кожен драйвер має власний маніфест з:
- Metadata (name, type, version)
- Capabilities (що може робити)
- Configuration schema
- Specific APIs
- UI extensions

### 2. Manager Manifest
Менеджер має маніфест з:
- Module metadata
- Driver registry configuration
- Base APIs (загальні для всіх драйверів)
- Dynamic API composition rules
- UI composition rules

### 3. Build-time Composition
Система збірки:
1. Сканує всі driver manifests
2. Композує їх в manager manifest
3. Генерує код реєстрації
4. Створює unified API

### 4. Runtime Behavior
- Manager ініціалізує всі драйвери
- Routing API calls до відповідних драйверів
- Агрегація даних від драйверів
- Unified error handling
## Приклад: SensorManager

### Драйвери
```
sensor_drivers/
├── ds18b20_driver_manifest.json
├── ntc_driver_manifest.json
├── gpio_driver_manifest.json
└── dht22_driver_manifest.json
```

### Композиція
```
SensorManager отримує:
- sensor.get_all (власний метод)
- sensor.ds18b20.set_resolution (від DS18B20)
- sensor.ntc.set_coefficients (від NTC)
- sensor.gpio.set_debounce (від GPIO)
```

### UI композиція
Базове меню "Датчики" розширюється полями від активних драйверів.

## Inter-module залежності

### ClimateManager → SensorManager
```json
{
  "dependencies": {
    "runtime": [
      {
        "module": "SensorManager",
        "required_apis": [
          "sensor.get_by_role"
        ]
      }
    ]
  }
}
```

### Це дозволяє
1. Build-time валідацію залежностей
2. Runtime dependency injection
3. Clear API contracts між модулями