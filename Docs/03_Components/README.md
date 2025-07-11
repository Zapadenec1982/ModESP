# 🔧 Components - Модулі та компоненти

Цей розділ містить документацію по всіх модулях та компонентах системи ModESP.

## 📁 Підрозділи

### [core/](core/README.md) - Основні системні модулі
Ядро системи: SharedState, EventBus, Configuration, Application lifecycle

**Ключові документи:**
- **Core System** - Application Controller, EventBus, SharedState API
- **ConfigManager** - 🆕 Modular configuration з lifecycle management
- **Lifecycle Architecture** - 🆕 BOOT→RUNTIME→SHUTDOWN фази
- **Async Optimization** - 🆕 LittleFS та watchdog-friendly операції

### [sensors/](sensors/README.md) - Модулі датчиків  
Системи збору даних: DS18B20, NTC, Pressure, GPIO inputs

### [actuators/](actuators/README.md) - Модулі актуаторів
Системи керування: Relay, PWM, GPIO outputs, Stepper motors

### [rtc/](rtc/README.md) - Real Time Clock модуль
Система часу та календаря з синхронізацією

### [logger/](logger/README.md) - Система логування
Файлова система, структуроване логування, HACCP сумісність

## 🎯 Архітектура модулів

### Загальні принципи
- **Модульність**: Кожен компонент - самодостатній модуль
- **Інтерфейси**: Стандартизовані API для всіх типів модулів
- **Конфігурація**: JSON-based налаштування без перекомпіляції
- **Автореєстрація**: Драйвери реєструються автоматично при старті

### Ієрархія модулів
```
Application (Main Loop)
    ↓
ModuleManager
    ↓
Core Modules (SharedState, EventBus, Config)
    ↓
Hardware Modules (SensorModule, ActuatorModule)
    ↓
Drivers (DS18B20, Relay, PWM, etc.)
    ↓
HAL (Hardware Abstraction Layer)
```

## 🔗 Взаємодія між модулями

### Через SharedState (рекомендовано)
```cpp
// Датчик публікує дані
SharedState::set("sensor.chamber.temperature", 22.5f);

// Актуатор читає дані для прийняття рішень
auto temp = SharedState::get<float>("sensor.chamber.temperature");
```

### Через EventBus (для подій)
```cpp
// Публікація події
EventBus::publish("alarm.temperature.high", {{"value", 35.0}});

// Підписка на подію
EventBus::subscribe("alarm.*", [](const Event& e) {
    handle_alarm(e);
});
```

## 📚 Документація по категоріях

### За складністю
- **Початківці**: [core/shared_state.md](core/shared_state.md) - основи обміну даними
- **Середній рівень**: [sensors/sensor_module.txt](sensors/sensor_module.txt) - робота з датчиками
- **Просунуті**: [core/core_system.md](core/core_system.md) - повна архітектура Core

### За функціональністю
- **Збір даних**: [sensors/](sensors/README.md) - всі типи датчиків
- **Керування**: [actuators/](actuators/README.md) - реле, PWM, моторы
- **Час та події**: [rtc/](rtc/README.md) - календар та розклад
- **Моніторинг**: [logger/](logger/README.md) - логування та метрики

## 🚀 Швидкий старт

### Додавання нового датчика
1. Прочитайте [sensors/sensor_module.txt](sensors/sensor_module.txt)
2. Реалізуйте `ISensorDriver` інтерфейс
3. Додайте автореєстрацію драйвера
4. Конфігуруйте в `sensors.json`

### Додавання нового актуатора
1. Прочитайте [actuators/actuator_module.txt](actuators/actuator_module.txt)
2. Реалізуйте `IActuatorDriver` інтерфейс  
3. Додайте автореєстрацію драйвера
4. Конфігуруйте в `actuators.json`

---

*Документація оновлена: 2025-06-29*
