# Core Modules - Основні системні модулі

Базові модулі що забезпечують функціонування всієї системи ModESP.

## 📚 Документи в цьому розділі

### Головні модулі
- **[core_system.md](core_system.md)** - 🆕 Повний довідник по Core системі
  - Application Controller, EventBus, SharedState, Config
  - Memory footprint та performance характеристики
  - API reference з прикладами
  - Communication patterns та best practices

- **[event_bus.md](event_bus.md)** - 🆕 EventBus - асинхронна система подій
  - Thread-safe publish-subscribe система
  - Пріоритетність та фільтрація подій
  - Pattern matching для підписок
  - Приклади використання та best practices

- **[shared_state.md](shared_state.md)** - 🆕 Централізоване сховище стану
  - Key-value storage з type safety
  - Thread-safe операції
  - Підписка на зміни (callback system)
  - Naming conventions та patterns

### ConfigManager система
- **[config_manager_overview.md](config_manager_overview.md)** - 🆕 ConfigManager overview
  - Modular configuration management
  - RAM caching та atomic saves
  - Lifecycle management
  - Async optimization features

- **[lifecycle_architecture.md](lifecycle_architecture.md)** - 🆕 Lifecycle архітектура
  - BOOT → RUNTIME → SHUTDOWN фази
  - Синхронні vs асинхронні операції
  - Правильний порядок завантаження/збереження

- **[async_optimization.md](async_optimization.md)** - 🆕 LittleFS оптимізація
  - Watchdog-friendly операції
  - Асинхронний запис та batching
  - Performance optimization

- **[config_quick_reference.md](config_quick_reference.md)** - 🆕 Швидкий довідник
  - API методи та приклади
  - Best practices конфігурації

- **[config_lifecycle_sequence.mmd](config_lifecycle_sequence.mmd)** - 🆕 Mermaid діаграми
  - Візуальні схеми lifecycle процесів

### Допоміжні модулі
- **[application_lifecycle.txt](application_lifecycle.txt)** - Lifecycle координатор
  - Керування життєвим циклом системи
  - Ініціалізація та shutdown процедури
  - Error handling та recovery

- **[config_manager.txt](config_manager.txt)** - Система конфігурації
  - JSON-based конфігурація
  - NVS persistence
  - Validation та defaults

- **[module_manager.txt](module_manager.txt)** - Управління модулями
  - Priority-based execution
  - Module registration та lifecycle
  - Coordination між модулями

- **[event_bus_example.cpp](event_bus_example.cpp)** - Приклад використання EventBus
  - Повний робочий приклад
  - Підписка та публікація подій
  - Обробка пріоритетів

## 🎯 Ключові концепції

### Core Principles
- **< 5KB RAM footprint** для всього ядра
- **Zero Dynamic Allocation** - вся пам'ять на старті
- **Explicit Dependencies** - ніяких hidden singletons
- **Fail-Safe** - система працює навіть при збоях модулів

### Архітектура
```
Application Controller (Main Loop @ 100Hz)
    ↓
ModuleManager (Priority-based execution)
    ↓
Core Services
├── SharedState (Data Exchange)
├── EventBus (Async Messaging) 
├── Config (Persistent Storage)
└── Diagnostic (Health Monitoring)
```

## 🔄 Потоки даних

### SharedState Pattern (основний)
```cpp
// Producer
SharedState::set("sensor.temperature", 22.5f);

// Consumer  
auto temp = SharedState::get<float>("sensor.temperature");
if (temp.has_value()) {
    process_temperature(temp.value());
}
```

### EventBus Pattern (для подій)
```cpp
// Publisher
EventBus::publish("alarm.high_temp", {{"value", 35.0}});

// Subscriber
EventBus::subscribe("alarm.*", [](const Event& e) {
    handle_alarm(e.type, e.data);
});
```

## 📊 Performance характеристики

| Компонент | Static RAM | Операції/с | Latency |
|-----------|------------|------------|---------|
| Application | 128-256B | 100Hz loop | 10ms |
| SharedState | 2.5-3.5KB | 10,000+ | <1ms |
| EventBus | 600-800B | 1,000+ | <5ms |
| Config | 256B | 10-100 | 10-50ms |

## 🔗 Зв'язки з іншими модулями

- **HAL**: [02_Architecture/hal_reference.md](../../02_Architecture/hal_reference.md)
- **Sensors**: [../sensors/](../sensors/README.md) - використовують SharedState
- **Actuators**: [../actuators/](../actuators/README.md) - підписані на SharedState
- **UI/API**: [../../05_UI_API/](../../05_UI_API/README.md) - читають з SharedState

## 🚀 Швидкий старт

### Для розуміння архітектури (30 хв)
1. **[core_system.md](core_system.md)** - загальний огляд
2. **[shared_state.md](shared_state.md)** - основи data exchange

### Для розробки (1-2 год)
1. Прочитайте Core API з [core_system.md](core_system.md)
2. Вивчіть patterns з [shared_state.md](shared_state.md)
3. Перейдіть до [../sensors/](../sensors/README.md) або [../actuators/](../actuators/README.md)

---

*Документація оновлена: 2025-06-29*
