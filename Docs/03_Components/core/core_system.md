# ModuChill Core - Complete Reference

## Overview & Philosophy

### What is ModuChill Core?

ModuChill Core is a minimal coordination layer for modular climate control systems on ESP32-S3. It provides **MECHANISMS, not POLICIES** - enabling modules to communicate and coordinate without imposing business logic.

### Core Principles

* **Minimal Core**: < 5KB RAM footprint for essential services
* **Zero Dynamic Allocation**: All memory allocated at startup
* **Explicit Dependencies**: No hidden singletons, dependencies injected via constructors
* **Static by Default**: Static instances over singleton patterns
* **Use Platform**: Leverages ESP-IDF, doesn't replace it
* **Fail-Safe**: System continues operating even when modules fail
* **Deterministic**: Predictable timing and behavior

## Architecture Components

* **Application Controller**: Lifecycle coordination, main loop @ 100Hz
* **EventBus**: Thread-safe async messaging with pattern matching
* **SharedState**: Type-safe centralized state storage with change tracking
* **Config**: JSON-based persistent configuration with validation
* **ModuleManager**: Module lifecycle with priority execution
* **UIManager**: Central UI coordinator that generates schema for different interfaces
* **Diagnostic Interfaces**: Standardized health and metrics reporting

## Memory Footprint

| Component | Static RAM |
|-----------|------------|
| Application | 128-256B |
| EventBus | 600-800B + queue (32 events default) |
| SharedState | 2.5-3.5KB (64 entries) |
| Config | 256B + 2KB stack during operations |
| ModuleManager | 256-512B |
| **Total Core** | **< 5KB static allocation** |

---

## Complete API Reference

### Application Controller

Controls system lifecycle and main loop execution.

**Lifecycle States**: `BOOT → INIT → RUNNING → ERROR/SHUTDOWN`

#### Core Methods:

* `init()` - Initialize hardware, NVS, logging, core services
* `run()` - Main loop @ 100Hz, blocks forever, calls ModuleManager::tick_all()
* `shutdown()` - Graceful shutdown in reverse init order
* `get_state()` - Current system state (BOOT/INIT/RUNNING/ERROR/SHUTDOWN)
* `is_running()` - Quick check if system operational
* `check_health()` - Run system-wide health check (1Hz in main loop)
* `report_error(component, error)` - Central error reporting
* `get_uptime_ms()` - Milliseconds since boot
* `get_free_heap()` - Available heap memory
* `get_cpu_usage()` - CPU utilization percentage

#### Main Loop Structure (10ms cycle):

* `ModuleManager::tick_all()` - Update all modules by priority
* `EventBus::process()` - Process queued events
* Health check (every 1s) - Monitor system health
* Watchdog feed - Prevent system reset

### EventBus

Thread-safe publish-subscribe system with pattern matching.

#### Publishing:

* `publish(type, data, priority)` - Publish event with optional priority
* **Priorities**: CRITICAL(0), HIGH(1), NORMAL(2), LOW(3)
* **Thread-safe**: can call from any task
* Events queued if called from non-main task

#### Subscription:

* `subscribe(pattern, handler)` - Subscribe to event pattern
* **Pattern matching**: `""` matches all, `"sensor."` matches `"sensor.temp"`, `"sensor.humidity"`
* **Handler signature**: `void(const Event&)` where Event has type, data, timestamp, priority
* `unsubscribe(pattern)` - Remove subscription
* **Main task only** for subscribe/unsubscribe

#### Processing:

* `process(max_ms)` - Process event queue for up to max_ms
* `get_queue_size()` - Current events in queue
* `clear()` - Clear all pending events
* `get_published_count()` - Total events published
* `get_dropped_count()` - Events dropped due to full queue

#### Event Structure:

```cpp
struct Event {
   std::string type;      // Event identifier
   json data;            // Payload (nlohmann::json)
   uint32_t timestamp;   // esp_timer_get_time() when published
   uint8_t priority;     // Execution priority
};
```

### SharedState

Type-safe key-value storage with change notifications.

#### Basic Operations:

* `set<T>(key, value)` - Store typed value
* `get<T>(key)` - Retrieve as std::optional<T>
* `exists(key)` - Check if key exists
* `remove(key)` - Delete key-value pair

#### Atomic Operations:

* `compare_and_swap(key, expected, new_val)` - CAS for floats
* `increment(key, delta)` - Atomic increment, default delta=1.0

#### Change Tracking:

* `has_changed(pattern, since_timestamp)` - Check if any matching keys changed
* `get_last_change_time(key)` - When key last modified

### Config

JSON-based configuration with NVS persistence and validation.

#### Lifecycle:

* `load()` - Load from NVS or use defaults
* `save()` - Persist to NVS with two-phase commit
* `reset_to_defaults()` - Factory reset

#### Access Methods:

* `get_all()` - Complete config as JSON
* `get_section(path)` - Get nested section (e.g., `"climate.control"`)
* `get<T>(path, default)` - Get typed value with default
* `set<T>(path, value)` - Set typed value

### ModuleManager

Priority-based module lifecycle coordination.

#### Registration:

* `register_module(module, priority)` - Add module with priority
* **Priority levels**: CRITICAL(0), HIGH(1), NORMAL(2), LOW(3)
* `unregister_module(name)` - Remove module

#### Lifecycle Control:

* `init_all()` - Initialize all modules in priority order
* `tick_all()` - Execute update() for all enabled modules
* `shutdown_all()` - Stop modules in reverse priority order

---

## Task Architecture & Threading

### Threading Model

* **Main Task**: Cooperative scheduling for modules (Core 1, priority 1)
* **Network Tasks**: WiFi, HTTP, MQTT on Core 0
* **Blocking I/O**: Separate tasks only when necessary (Core 1, priority 15)
* **Critical Tasks**: Watchdog, safety systems (Core 1, priority 20)

---

## Communication Patterns

### Event-Driven Pattern

```cpp
// Publisher
EventBus::publish("sensor.reading", {
   {"id", "temp1"}, 
   {"value", 25.5},
   {"unit", "celsius"}
});

// Subscriber
EventBus::subscribe("sensor.*", [](const Event& e) {
   float value = e.data["value"];
   process_reading(value);
});
```

### State Observer Pattern

```cpp
// Producer updates state
SharedState::set("runtime.climate.temperature", 22.5f);

// Consumer polls for changes
void update() {
   static uint32_t last_check = 0;
   if (SharedState::has_changed("runtime.*", last_check)) {
       auto temp = SharedState::get<float>("runtime.climate.temperature");
       update_display(temp.value_or(0));
       last_check = esp_timer_get_time();
   }
}
```

### Schema-Driven UI Pattern (Рекомендований для UI)

```
                                        ┌──────────────┐
WebServer ┐                             │ SharedState  │
         ├─> UIManager ─(read state)──>├──────────────┤
Display   ┘   (generates UI schema)     │ ConfigManager│
           <─(provides UI schema)───   └──────────────┘
```

**Процес роботи**:

1. **Запит Схеми**: WebServer або DisplayModule запитує UI-схему у UIManager
2. **Агрегація Даних**: UIManager збирає оперативні дані з SharedState та налаштування з ConfigManager
3. **Генерація Схеми**: UIManager створює JSON-схему, що описує вигляд та стан інтерфейсу
4. **Рендеринг**: WebServer рендерить схему як HTML, DisplayModule - на екрані
5. **Дії Користувача**: Всі дії (натискання кнопок, зміна значень) надсилаються як команди до UIManager, який вже вирішує, як оновити ConfigManager чи SharedState

---

## Error Handling & Recovery

### Error Categories

* **Configuration**: Invalid JSON, missing fields, out of range
* **Initialization**: Hardware failures, resource allocation
* **Runtime**: Exceptions, timeouts, deadline violations
* **Resource**: Memory exhaustion, stack overflow

---

## Best Practices Summary

* Single responsibility per module
* Explicit dependency injection
* Fail gracefully on errors
* Static memory allocation
* Bounded execution time
* Appropriate logging levels

---

*Документ конвертований з Core.txt в markdown формат для кращої читабельності та консистентності з рештою документації.*
