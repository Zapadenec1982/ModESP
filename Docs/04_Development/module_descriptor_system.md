# 📋 Module Descriptor System - Документація модулів ModESP

## 🎯 Мета системи

**Module Descriptor System** - це простий спосіб документування та зручної роботи з модулями ModESP. Кожен модуль має **інформаційний файл** що описує його API, UI та інтеграційні можливості.

### 🌟 Основні переваги

- **Централізована документація** - вся інформація про модуль в одному файлі
- **Легкий пошук API** - розробники швидко знаходять потрібні методи
- **Зручне керування** - простий спосіб вмикати/вимикати модулі
- **Приклади інтеграції** - готові фрагменти коду для розробників
- **Самодокументованість** - модулі описують свої можливості

> ⚠️ **Важливо**: Ця система **НЕ змінює** існуючу архітектуру ModESP! 
> ModuleManager + module_registry працюють як раніше. Дескриптори - це тільки інформація.

## 🏗️ Архітектура (існуюча зберігається)

```
Existing Architecture (UNCHANGED)     New Information Layer
                                      
┌─────────────────┐                   ┌─────────────────────┐
│ ModuleManager   │ ← (same as before) │ module_descriptor   │
│                 │                   │       .json         │
│ - Lifecycle     │                   │                     │
│ - Priorities    │                   │ - API documentation │
│ - Performance   │                   │ - Integration guide │
│ - Health        │                   │ - UI description    │
└─────────────────┘                   │ - Configuration     │
         ▲                            └─────────────────────┘
         │                                       │
┌─────────────────┐                            │
│ module_registry │ ← (same as before)          │
│                 │                            │
│ - Registers     │                            ▼
│   modules       │                   ┌─────────────────────┐
│ - Sets          │                   │ tools/module_info   │
│   priorities    │                   │       .py           │
└─────────────────┘                   │                     │
                                      │ - List modules      │
                                      │ - Show API          │
                                      │ - Enable/disable    │
                                      │ - Integration guide │
                                      └─────────────────────┘
```

## 📄 Формат Module Descriptor

### Базова структура
```json
{
    "module_info": {
        "name": "SensorModule",
        "version": "1.2.0",
        "description": "Sensor data collection module",
        "enabled": true,
        "priority": "HIGH"
    },
    
    "api_reference": {
        "rpc_methods": { /* RPC методи з прикладами */ },
        "shared_state_keys": { /* SharedState ключі */ },
        "events_published": { /* EventBus події */ },
        "events_subscribed": { /* EventBus підписки */ }
    },
    
    "ui_schema": {
        "web": { /* Веб інтерфейс */ },
        "mqtt": { /* MQTT топіки */ }
    },
    
    "integration_guide": {
        "dependencies": ["ESPhal", "SharedState"],
        "how_to_interact": { /* Приклади коду */ }
    }
}
```

## 🔧 Секції дескриптора

### 1. Module Info
```json
"module_info": {
    "name": "SensorModule",
    "version": "1.2.0", 
    "description": "Comprehensive sensor data collection module",
    "enabled": true,        // ← Легке вкл/викл
    "priority": "HIGH",
    "dependencies": ["ESPhal", "SharedState", "EventBus"]
}
```

### 2. API Reference - для розробників
```json
"api_reference": {
    "rpc_methods": {
        "sensor.get_temperature": {
            "description": "Get current temperature reading",
            "params": {"sensor_id": "string (optional)"},
            "returns": {"value": "float", "unit": "°C"},
            "example": "sensor.get_temperature() -> {value: 23.5, unit: '°C'}"
        }
    },
    
    "shared_state_keys": {
        "state.sensor.temperature": {
            "type": "float",
            "description": "Current temperature in Celsius", 
            "update_frequency": "1 Hz"
        }
    },
    
    "events_published": {
        "sensor.reading_updated": {
            "description": "New sensor reading available",
            "payload": {"sensor_id": "string", "value": "float"},
            "frequency": "1 Hz"
        }
    }
}
```

### 3. Integration Guide - як використовувати
```json
"integration_guide": {
    "how_to_interact": {
        "from_other_modules": {
            "read_temperature": "float temp = SharedState::get<float>(\"state.sensor.temperature\");",
            "listen_for_updates": "EventBus::subscribe(\"sensor.reading_updated\", my_handler);",
            "call_rpc": "JsonRpc::call(\"sensor.get_temperature\", {});"
        },
        "for_new_developers": {
            "understanding": "This module collects data from various sensors",
            "key_files": ["sensor_module.h", "sensor_module.cpp"],
            "testing": "Use sensor.get_temperature() RPC to verify module works"
        }
    }
}
```

## 🛠️ Інструменти для розробників

### Module Info Tool
```bash
# Показати всі доступні модулі  
python tools/module_info.py --list

# Детальна API документація модуля
python tools/module_info.py --api sensor_drivers

# Гайд по інтеграції з модулем
python tools/module_info.py --guide sensor_drivers

# Вимкнути модуль (змінює enabled: false)
python tools/module_info.py --disable sensor_drivers

# Включити модуль
python tools/module_info.py --enable sensor_drivers
```

## 🎯 Практичне використання

### Для нових розробників
1. **Дослідити** доступні модулі: `python tools/module_info.py --list`
2. **Вивчити API** потрібного модуля: `--api module_name`
3. **Побачити приклади** інтеграції: `--guide module_name`
4. **Використати** готові фрагменти коду

### Для досвідчених розробників  
1. **Швидко знайти** потрібний RPC метод чи SharedState ключ
2. **Перевірити** формат EventBus повідомлень
3. **Вимкнути** непотрібні модулі для тестування
4. **Документувати** власні модулі за тим же форматом

## 📂 Розміщення файлів

```
components/
├── sensor_drivers/
│   ├── module_descriptor.json     ← Повна документація модуля
│   ├── sensor_module.h
│   └── sensor_module.cpp
├── actuator_drivers/
│   ├── module_descriptor.json
│   └── ...
└── core/
    ├── module_manager.cpp         ← Без змін!
    └── module_registry.cpp        ← Без змін!

tools/
└── module_info.py                 ← Читає дескриптори
```

## ✅ Що система НЕ робить

- ❌ **НЕ змінює** ModuleManager 
- ❌ **НЕ замінює** module_registry
- ❌ **НЕ автогенерує** код модулів
- ❌ **НЕ ламає** існуючу архітектуру

## ✅ Що система робить

- ✅ **Документує** API модулів в одному місці
- ✅ **Спрощує** розробку через приклади
- ✅ **Дозволяє** легко вмикати/вимикати модулі
- ✅ **Допомагає** новим розробникам
- ✅ **Розширює** існуючий ui_generator.py підхід

---

> 💡 **Підсумок**: Module Descriptor System - це інформаційний шар поверх існуючої архітектури ModESP, що робить роботу з модулями зручнішою для розробників, не змінюючи при цьому core системи.
