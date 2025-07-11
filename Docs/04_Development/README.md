# 🛠️ ModESP Development Guide

Ласкаво просимо до розробки ModESP! Цей каталог містить всю необхідну інформацію для ефективної розробки.

## 📚 Документація розробника

### 🎯 Основи розробки
- **[Module Descriptor System](module_descriptor_system.md)** - Документація та керування модулями
- **[Architecture Overview](../02_Architecture/README.md)** - Архітектура системи

### 🔧 Практичні інструменти
- **Module Info Tool** - Дослідження API модулів
- **Configuration Management** - Керування конфігурацією
- **Testing Framework** - Тестування компонентів
- **UI Generator** - Автоматична генерація інтерфейсів

## 🚀 Швидкий старт для розробників

### 1. Дослідження існуючих модулів
```bash
# Переглянути всі доступні модулі
python tools/module_info.py --list

# Вивчити API конкретного модуля  
python tools/module_info.py --api sensor_drivers

# Побачити приклади інтеграції
python tools/module_info.py --guide sensor_drivers
```

### 2. Розуміння архітектури
- **ModuleManager** - керує lifecycle та performance модулів
- **module_registry** - реєструє модулі в системі
- **SharedState** - обмін даними між модулями
- **EventBus** - асинхронна комунікація
- **JsonRPC** - віддалене керування

### 3. Створення нового модуля
1. Створити модуль, що наслідує `BaseModule`
2. Зареєструвати в `module_registry.cpp`
3. Додати `module_descriptor.json` для документації
4. Протестувати інтеграцію

## 📋 Module Descriptor System

### Що це дає розробникам:
- **Швидкий пошук API** - всі методи, події, ключі в одному файлі
- **Готові приклади** - copy-paste код для інтеграції
- **Зручне керування** - вимкнути/включити модулі одною командою
- **Самодокументованість** - модулі описують свої можливості

### Приклад дескриптора:
```json
{
    "module_info": {
        "name": "SensorModule",
        "enabled": true,
        "priority": "HIGH"
    },
    "api_reference": {
        "rpc_methods": {
            "sensor.get_temperature": {
                "description": "Get current temperature",
                "example": "sensor.get_temperature() -> {value: 23.5}"
            }
        },
        "shared_state_keys": {
            "state.sensor.temperature": {
                "type": "float",
                "description": "Current temperature in °C"
            }
        }
    },
    "integration_guide": {
        "how_to_interact": {
            "read_data": "float temp = SharedState::get<float>(\"state.sensor.temperature\");"
        }
    }
}
```

## 🔧 Практичні інструменти

### Module Info Tool
Основний інструмент для роботи з модулями:
```bash
python tools/module_info.py --help
```

### UI Generator  
Генерує веб-інтерфейс з module descriptors:
```bash  
python tools/ui_generator.py
```

## 🏗️ Архітектурні принципи

1. **Модульність** - кожен компонент незалежний
2. **Слабка зв'язаність** - модулі взаємодіють через APIs
3. **Документованість** - всі інтерфейси описані  
4. **Тестованість** - кожен модуль має тести
5. **Конфігурованість** - поведінка керується конфігурацією

## 📁 Структура проекту

```
ModESP/
├── components/           # Модулі системи
│   ├── sensor_drivers/   
│   │   ├── module_descriptor.json  ← API документація
│   │   ├── sensor_module.h
│   │   └── sensor_module.cpp
│   └── core/
│       ├── module_manager.cpp      ← Керування модулями
│       └── module_registry.cpp     ← Реєстрація модулів
├── tools/                # Інструменти розробки
│   ├── module_info.py    ← Дослідження модулів
│   └── ui_generator.py   ← Генерація UI
└── Docs/                 # Документація
    ├── 02_Architecture/  ← Архітектура
    └── 04_Development/   ← Розробка
```

## 🎯 Найкращі практики

### Для нових розробників:
1. Почати з дослідження існуючих модулів через `module_info.py`
2. Вивчити приклади в integration_guide
3. Створити простий модуль за зразком
4. Додати документацію в дескриптор

### Для досвідчених розробників:
1. Використовувати дескриптори для швидкого пошуку API
2. Тестувати модулі ізольовано
3. Документувати всі інтерфейси
4. Оптимізувати performance через ModuleManager

### 💡 Поради щодо використання JSON

- **SharedState/EventBus**: Намагайтеся не зберігати надто великі або глибоко вкладені JSON-об'єкти. Це допоможе контролювати використання пам'яті.
- **Конфігурація**: Розбивайте конфігурацію на логічні модульні файли.
- **Продуктивність**: Уникайте парсингу великих JSON-рядків у методах `update()`, які викликаються часто. Використовуйте для цього `configure()` або обробники подій.

---

> 🚀 **ModESP Module Descriptor System** робить розробку швидшою та зручнішою, не змінюючи при цьому надійну архітектуру системи!
