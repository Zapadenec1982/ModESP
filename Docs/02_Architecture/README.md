# 🏗️ Architecture - Архітектура системи

Системна архітектура, основні концепції та Module Descriptor System для документації модулів.

## 📚 Документи

### Системна архітектура
- **[system_architecture.md](system_architecture.md)** - Повна архітектура системи ModESP
- **[api_contracts.md](api_contracts.md)** - Системні контракти та інтерфейси

### HAL (Hardware Abstraction Layer)
- **[hal_reference.md](hal_reference.md)** - 🆕 Об'єднаний HAL довідник
- **[hal_development_summary.md](hal_development_summary.md)** - Підсумок розробки HAL

### Розробка модулів
- **[Module Descriptor System](../04_Development/module_descriptor_system.md)** - Документація та керування модулями

## 🎯 Ключові архітектурні принципи

### Module Descriptor System
- **Інформаційний шар** - модулі описують свої можливості
- **Документованість** - централізована API документація
- **Зручність розробки** - швидкий пошук інтерфейсів
- **Зворотна сумісність** - без змін існуючої архітектури

### Основні принципи
- **Модульність** - кожен компонент має чітку відповідальність
- **HAL абстракція** - незалежність від конкретного залізного забезпечення
- **Event-driven** - асинхронна комунікація між модулями
- **Resource management** - ефективне використання пам'яті та CPU

## 🏛️ Архітектурні рівні

```
┌─────────────────────────────────────────┐
│           Application Layer             │ ← Бізнес логіка
├─────────────────────────────────────────┤
│           Module Layer                  │ ← Модулі з дескрипторами  
├─────────────────────────────────────────┤
│           Core Services                 │ ← SharedState, EventBus
├─────────────────────────────────────────┤
│           HAL (Hardware Abstraction)    │ ← Hardware independence
├─────────────────────────────────────────┤
│           ESP-IDF / FreeRTOS            │ ← System foundation
└─────────────────────────────────────────┘
```

## 🔄 Module Lifecycle

1. **Registration**: ModuleRegistry реєструє модулі в ModuleManager
2. **Configuration**: Завантаження конфігурації з JSON файлів
3. **Initialization**: Ініціалізація модулів за пріоритетами
4. **Runtime**: Виконання модулів з моніторингом performance
5. **Shutdown**: Коректне зупинення в зворотному порядку

## 🎯 Ключові концепції

### Модульність
- **ModuleManager** - керування lifecycle та performance
- **module_registry** - реєстрація модулів в системі
- **JSON конфігурація** без перекомпіляції
- **module_descriptor.json** - документація API

### Симетрична архітектура
```
SensorModule ↔ SharedState ↔ ActuatorModule
     ↕                            ↕
SensorDrivers                ActuatorDrivers
     ↕                            ↕
         HAL (Hardware Abstraction Layer)
```

### Потоки даних
1. **Датчики → SharedState**: `Sensor → Driver → Module → SharedState`
2. **SharedState → Актуатори**: `SharedState → Module → Driver → Actuator`
3. **Бізнес-логіка**: Читає з SharedState, приймає рішення, пише команди

## 🔧 Module Management Architecture

```
┌─────────────────┐    ┌─────────────────────┐    ┌─────────────────┐
│ ModuleManager   │    │ module_registry     │    │ module_descriptor│
│                 │    │                     │    │      .json      │
│ - Lifecycle     │◄───┤ - register_all()    │    │                 │
│ - Priorities    │    │ - Sets priorities   │    │ - API docs      │
│ - Performance   │    │ - Module creation   │    │ - Integration   │ 
│ - Health        │    │                     │    │ - Examples      │
└─────────────────┘    └─────────────────────┘    └─────────────────┘
         │                                                 │
         │                                                 │
         ▼                                                 ▼
┌─────────────────┐                              ┌─────────────────┐
│ BaseModule      │                              │ tools/          │
│                 │                              │ module_info.py  │
│ - init()        │                              │                 │
│ - update()      │                              │ - List modules  │
│ - register_rpc()│                              │ - Show API      │
└─────────────────┘                              │ - Enable/disable│
                                                 └─────────────────┘
```

## 🔗 Зв'язки з іншими розділами

- **Компоненти**: [03_Components/](../03_Components/README.md) - реалізація модулів
- **UI/API**: [05_UI_API/](../05_UI_API/README.md) - інтерфейси користувача
- **Конфігурація**: [06_Configuration/](../06_Configuration/README.md) - налаштування системи
- **Розробка**: [04_Development/](../04_Development/README.md) - інструменти розробника

## 📖 Рекомендований порядок вивчення

### Базовий рівень (1-2 години)
1. **[system_architecture.md](system_architecture.md)** - загальне розуміння
2. **[api_contracts.md](api_contracts.md)** - контракти між модулями
3. **[Module Descriptor System](../04_Development/module_descriptor_system.md)** - розуміння документації
4. Перехід до [03_Components/core/](../03_Components/core/README.md)

### Поглиблений рівень (2-3 години)  
1. **[hal_reference.md](hal_reference.md)** - HAL інтерфейси
2. **[hal_development_summary.md](hal_development_summary.md)** - останні зміни
3. Практика з `python tools/module_info.py`
4. Перехід до [03_Components/](../03_Components/README.md) - конкретні модулі

---

*Документація оновлена: 2025-07-01*
