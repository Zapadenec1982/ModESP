# SharedState - Централізоване сховище стану

## Що це і навіщо

Централізоване сховище стану системи ModuChill. Модулі читають/пишуть дані без знання один про одного. Спеціально розроблено для промислових холодильних систем з акцентом на надійність та діагностику.

## Ключові концепції

* **JSON-based сховище**: Зберігає JSON значення для максимальної гнучкості
* **ESP-IDF інтеграція**: Використовує `esp_err_t` для надійної обробки помилок
* **Thread-safe**: Захищено м'ютексом. Можна викликати з будь-якої task (крім ISR)
* **Підписка на зміни**: Модулі отримують callback при зміні значення
* **No dynamic allocation**: Фіксований розмір сховища. Вся пам'ять виділена при старті
* **Повна діагностика**: Детальна статистика для промислового моніторингу

## Архітектура

```
┌─────────────────────────────────────────────────────┐
│                  SharedState                        │
├─────────────────────────────────────────────────────┤
│                                                     │
│  Storage (compile-time configurable, default 64):  │
│  ┌─────────────┬─────────────┬─────────────┐       │
│  │ Key (32B)   │ JSON Value  │ Meta (16B)  │       │
│  └─────────────┴─────────────┴─────────────┘       │
│                                                     │
│  FreeRTOS Mutex → Захист доступу                   │
│                                                     │
│  Subscription Manager:                              │
│  - Pattern → Список callbacks                      │
│  - Handle → Pattern (для відписки)                  │
│                                                     │
│  При set():                                         │
│  1. Lock mutex                                      │
│  2. Оновити значення                                │
│  3. Скопіювати список callbacks                     │
│  4. Unlock mutex                                    │
│  5. Викликати callbacks (поза mutex!)               │
└─────────────────────────────────────────────────────┘
```

## Критичні обмеження

* **Конфігурований розмір**: Максимальна кількість записів є фіксованою на час роботи для надійності, але може бути налаштована на етапі компіляції (за замовчуванням 64). Це дозволяє адаптувати систему під різні конфігурації обладнання
* **Ключ до 32 символів** - довші обрізаються
* **JSON значення** - обмежені лише доступною пам'яттю ESP32
* **Callbacks синхронні** - блокують інші callbacks, але захищені від винятків
* **НЕ викликати з ISR** - використовувати чергу

## Naming Convention для ключів

* `sensor.location.property` - дані датчиків (наприклад: `sensor.chamber.temperature`)
* `actuator.name.property` - стан актуаторів (наприклад: `actuator.compressor.state`)
* `command.actuator.name` - команди керування (наприклад: `command.actuator.relay1`)
* `config.module.setting` - конфігураційні параметри
* `metrics.component.metric` - метрики компонентів для діагностики
* `system.state.property` - системні стани (наприклад: `system.state.mode`)
* `alarm.type.property` - аварійні сигнали

## Callback модель

* **Виклик одразу після set()** - не асинхронно!
* **Поза mutex** - можна викликати SharedState в callback
* **В тому ж потоці** - хто викликав set()
* **Захищено від винятків** - використовує `ModESP::safe_execute`


## API Reference

### Основні операції

```cpp
// Запис JSON значень
esp_err_t set(const std::string& key, const nlohmann::json& value);

// Читання JSON значень
esp_err_t get(const std::string& key, nlohmann::json& value);

// Helper методи для простих типів
template<typename T>
esp_err_t set_typed(const std::string& key, T value);

template<typename T>
std::optional<T> get_typed(const std::string& key);

// Перевірка існування
bool exists(const std::string& key);

// Видалення
esp_err_t remove(const std::string& key);
```

### Атомарні операції

```cpp
// Compare-and-set для безпечних оновлень
esp_err_t compare_and_set(const std::string& key, 
                         const nlohmann::json& expected, 
                         const nlohmann::json& new_value);

// Атомарний інкремент для лічильників
esp_err_t increment(const std::string& key, double delta = 1.0);
```

### Підписка на зміни

```cpp
// Підписка на зміни ключів за патерном
using ChangeCallback = std::function<void(const std::string& key, const nlohmann::json& value)>;
SubscriptionHandle subscribe(const std::string& pattern, ChangeCallback callback);

// Відписка
esp_err_t unsubscribe(SubscriptionHandle handle);
```

### Відстеження змін

```cpp
// Перевірка чи змінилися ключі з певного часу (64-bit timestamp)
bool has_changed(const std::string& pattern, uint64_t since_timestamp);

// Час останньої зміни ключа (мікросекунди з boot)
uint64_t get_last_update_time(const std::string& key);
```

### Статистика та діагностика

```cpp
// Структура статистики
struct Stats {
    size_t capacity;        // Максимальна кількість записів
    size_t used;           // Поточна кількість записів
    size_t peak_used;      // Пікове використання
    size_t total_sets;     // Загальна кількість записів
    size_t total_gets;     // Загальна кількість читань
    size_t subscriptions;  // Активні підписки
};

// Отримання повної статистики
Stats get_stats();

// Швидкі методи
size_t get_entry_count();
size_t get_subscription_count();
size_t get_total_sets();
size_t get_total_gets();

// Отримання всіх ключів за патерном
std::vector<std::string> get_keys(const std::string& pattern = "");

// Очищення всіх записів (для тестування)
void clear();
```


## Приклади використання в промисловому обладнанні

### Робота з датчиками температури

```cpp
// Публікація складних сенсорних даних
void publish_temperature_reading(float temperature, const std::string& sensor_id) {
    nlohmann::json sensor_data = {
        {"value", temperature},
        {"unit", "celsius"},
        {"timestamp", esp_timer_get_time()},
        {"quality", temperature > -50.0 && temperature < 80.0 ? "good" : "bad"},
        {"alarm", temperature < -25.0 || temperature > 10.0},
        {"calibration", {
            {"offset", 0.1},
            {"last_cal_date", "2025-01-15"},
            {"cal_certificate", "CERT-2025-001"}
        }},
        {"diagnostics", {
            {"wire_resistance", 1200.5},
            {"noise_level", 0.02},
            {"response_time_ms", 1500}
        }}
    };
    
    std::string key = "sensor." + sensor_id + ".temperature";
    esp_err_t err = SharedState::set(key, sensor_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to publish temperature for %s: %s", 
                 sensor_id.c_str(), esp_err_to_name(err));
        // Інкремент лічильника помилок
        SharedState::increment("metrics.sensor.publish_errors", 1.0);
    }
}

// Читання температури з перевіркою якості
std::optional<float> get_validated_temperature(const std::string& sensor_id) {
    std::string key = "sensor." + sensor_id + ".temperature";
    nlohmann::json data;
    
    if (SharedState::get(key, data) == ESP_OK) {
        if (data.contains("quality") && data["quality"] == "good") {
            return data["value"].get<float>();
        } else {
            ESP_LOGW(TAG, "Temperature quality bad for %s", sensor_id.c_str());
            SharedState::increment("metrics.sensor.quality_failures", 1.0);
        }
    }
    return std::nullopt;
}
```

### Керування актуаторами з безпекою

```cpp
// Команди з контролем безпеки та тайм-аутами
void command_compressor(bool state, const std::string& source) {
    nlohmann::json command = {
        {"state", state},
        {"source", source},
        {"timestamp", esp_timer_get_time()},
        {"priority", "high"},
        {"safety_checks", {
            {"min_off_time_sec", 300},    // Мінімум 5 хвилин між пусками
            {"max_runtime_sec", 3600},    // Максимум 1 година безперервної роботи
            {"pressure_check", true}
        }},
        {"timeout_ms", 30000},            // Команда діє 30 секунд
        {"retry_count", 3}
    };
    
    esp_err_t err = SharedState::set("command.actuator.compressor", command);
    if (err == ESP_OK) {
        SharedState::increment("metrics.actuator.commands_sent", 1.0);
        ESP_LOGI(TAG, "Compressor command sent: %s by %s", 
                 state ? "ON" : "OFF", source.c_str());
    } else {
        SharedState::increment("metrics.actuator.command_failures", 1.0);
        ESP_LOGE(TAG, "Failed to send compressor command: %s", esp_err_to_name(err));
    }
}

// Статус актуатора з діагностикою
void publish_actuator_status(const std::string& actuator_name, bool state, 
                           float current_draw, uint32_t cycles) {
    nlohmann::json status = {
        {"state", state},
        {"current_draw_A", current_draw},
        {"cycles_total", cycles},
        {"last_state_change", esp_timer_get_time()},
        {"diagnostics", {
            {"overcurrent", current_draw > 15.0},
            {"undercurrent", state && current_draw < 2.0},
            {"cycle_wear_percent", (cycles / 100000.0) * 100}  // 100k циклів = 100%
        }},
        {"maintenance", {
            {"next_service_cycles", 100000 - cycles},
            {"efficiency_percent", 95.0 - (cycles / 10000.0)}  // Падіння ефективності
        }}
    };
    
    std::string key = "actuator." + actuator_name + ".status";
    SharedState::set(key, status);
}
```

### Підписка на критичні зміни

```cpp
// Підписка на аварійні ситуації
class SafetyMonitor {
private:
    SubscriptionHandle temp_handle_;
    SubscriptionHandle actuator_handle_;
    
public:
    void init() {
        // Моніторинг критичної температури
        temp_handle_ = SharedState::subscribe("sensor.*.temperature", 
            [this](const std::string& key, const nlohmann::json& value) {
                if (value.contains("alarm") && value["alarm"] == true) {
                    handle_temperature_alarm(key, value);
                }
            });
        
        // Моніторинг стану актуаторів
        actuator_handle_ = SharedState::subscribe("actuator.*.status",
            [this](const std::string& key, const nlohmann::json& value) {
                check_actuator_health(key, value);
            });
        
        ESP_LOGI(TAG, "Safety monitor initialized");
    }
    
    void handle_temperature_alarm(const std::string& key, const nlohmann::json& data) {
        float temp = data["value"].get<float>();
        ESP_LOGW(TAG, "TEMPERATURE ALARM: %s = %.1f°C", key.c_str(), temp);
        
        // Автоматичні дії безпеки
        if (temp > 10.0) {
            // Критична температура - аварійне відключення
            command_compressor(true, "emergency_cooling");
            SharedState::set("system.state.emergency", true);
        }
        
        // Інкремент лічильника аварій
        SharedState::increment("metrics.safety.temperature_alarms", 1.0);
    }
    
    void check_actuator_health(const std::string& key, const nlohmann::json& data) {
        if (data.contains("diagnostics")) {
            auto diag = data["diagnostics"];
            
            if (diag.contains("overcurrent") && diag["overcurrent"] == true) {
                ESP_LOGE(TAG, "OVERCURRENT detected on %s", key.c_str());
                SharedState::increment("metrics.safety.overcurrent_events", 1.0);
                
                // Аварійне відключення
                std::string actuator_name = extract_actuator_name(key);
                emergency_shutdown(actuator_name);
            }
        }
    }
};
```


### Системна діагностика та метрики

```cpp
// Моніторинг системних ресурсів
void system_health_monitor() {
    // Отримання статистики SharedState
    auto stats = SharedState::get_stats();
    
    // Перевірка використання пам'яті
    if (stats.used > stats.capacity * 0.9) {
        ESP_LOGW(TAG, "SharedState almost full: %zu/%zu entries", 
                 stats.used, stats.capacity);
        SharedState::increment("metrics.system.memory_warnings", 1.0);
    }
    
    // Публікація системних метрик
    nlohmann::json system_metrics = {
        {"shared_state", {
            {"entries_used", stats.used},
            {"entries_capacity", stats.capacity},
            {"peak_usage", stats.peak_used},
            {"total_operations", stats.total_sets + stats.total_gets},
            {"active_subscriptions", stats.subscriptions},
            {"utilization_percent", (float)stats.used / stats.capacity * 100.0f}
        }},
        {"esp32", {
            {"free_heap", esp_get_free_heap_size()},
            {"min_free_heap", esp_get_minimum_free_heap_size()},
            {"uptime_sec", esp_timer_get_time() / 1000000},
            {"cpu_freq_mhz", esp_clk_cpu_freq() / 1000000}
        }}
    };
    
    SharedState::set("metrics.system.health", system_metrics);
    
    ESP_LOGI(TAG, "System Health: %zu/%zu entries, %zu bytes free heap, %.1f%% util",
             stats.used, stats.capacity, esp_get_free_heap_size(),
             (float)stats.used / stats.capacity * 100.0f);
}

// Аналіз продуктивності
void analyze_performance() {
    static uint64_t last_check = 0;
    static size_t last_sets = 0, last_gets = 0;
    
    auto stats = SharedState::get_stats();
    uint64_t now = esp_timer_get_time();
    
    if (last_check > 0) {
        uint64_t time_diff_sec = (now - last_check) / 1000000;
        size_t sets_per_sec = (stats.total_sets - last_sets) / time_diff_sec;
        size_t gets_per_sec = (stats.total_gets - last_gets) / time_diff_sec;
        
        nlohmann::json perf_data = {
            {"sets_per_second", sets_per_sec},
            {"gets_per_second", gets_per_sec},
            {"total_ops_per_second", sets_per_sec + gets_per_sec},
            {"avg_entries", stats.used}
        };
        
        SharedState::set("metrics.performance.shared_state", perf_data);
        
        ESP_LOGD(TAG, "Performance: %zu sets/s, %zu gets/s", sets_per_sec, gets_per_sec);
    }
    
    last_check = now;
    last_sets = stats.total_sets;
    last_gets = stats.total_gets;
}
```

## Patterns і Best Practices

### Publisher-Subscriber Pattern

```cpp
// Publisher (SensorModule)
class SensorModule {
public:
    void update() {
        for (auto& sensor : sensors_) {
            auto reading = sensor.read();
            if (reading.has_value()) {
                publish_sensor_reading(sensor.get_id(), reading.value());
            }
        }
    }
    
private:
    void publish_sensor_reading(const std::string& id, const SensorReading& reading) {
        std::string key = "sensor." + id + ".reading";
        SharedState::set(key, reading.to_json());
    }
};

// Subscriber (ClimateControl) 
class ClimateControl {
public:
    void init() {
        // Підписка на всі температурні датчики
        temp_subscription_ = SharedState::subscribe("sensor.*.temperature", 
            [this](const std::string& key, const nlohmann::json& value) {
                on_temperature_change(key, value);
            });
    }
    
private:
    void on_temperature_change(const std::string& key, const nlohmann::json& data) {
        if (data.contains("value") && data.contains("quality")) {
            if (data["quality"] == "good") {
                float temp = data["value"].get<float>();
                update_control_logic(key, temp);
            }
        }
    }
    
    SubscriptionHandle temp_subscription_;
};
```

### State Machine Pattern з SharedState

```cpp
enum class SystemState { 
    BOOT, INIT, RUNNING, DEFROST, ERROR, SHUTDOWN 
};

class SystemStateMachine {
public:
    void set_state(SystemState new_state, const std::string& reason) {
        SystemState old_state = current_state_;
        current_state_ = new_state;
        
        nlohmann::json state_data = {
            {"state", static_cast<int>(new_state)},
            {"state_name", state_to_string(new_state)},
            {"previous_state", static_cast<int>(old_state)},
            {"transition_reason", reason},
            {"timestamp", esp_timer_get_time()},
            {"uptime_sec", esp_timer_get_time() / 1000000}
        };
        
        SharedState::set("system.state.current", state_data);
        
        ESP_LOGI(TAG, "State transition: %s -> %s (%s)", 
                 state_to_string(old_state), state_to_string(new_state), 
                 reason.c_str());
        
        // Статистика переходів
        std::string transition_key = "metrics.state_transitions." + 
                                   state_to_string(new_state);
        SharedState::increment(transition_key, 1.0);
    }
    
    SystemState get_state() const { return current_state_; }
    
private:
    SystemState current_state_ = SystemState::BOOT;
    
    const char* state_to_string(SystemState state) {
        switch(state) {
            case SystemState::BOOT: return "BOOT";
            case SystemState::INIT: return "INIT"; 
            case SystemState::RUNNING: return "RUNNING";
            case SystemState::DEFROST: return "DEFROST";
            case SystemState::ERROR: return "ERROR";
            case SystemState::SHUTDOWN: return "SHUTDOWN";
            default: return "UNKNOWN";
        }
    }
};
```


### Configuration Management Pattern

```cpp
// Завантаження та відстеження конфігурації
class ConfigurableModule {
public:
    void init() {
        // Завантаження початкової конфігурації
        load_config();
        
        // Підписка на зміни конфігурації
        config_subscription_ = SharedState::subscribe("config.climate.*",
            [this](const std::string& key, const nlohmann::json& value) {
                on_config_change(key, value);
            });
    }
    
private:
    void load_config() {
        // Завантаження з ConfigManager або значення за замовчуванням
        setpoint_ = SharedState::get_typed<float>("config.climate.setpoint").value_or(4.0f);
        hysteresis_ = SharedState::get_typed<float>("config.climate.hysteresis").value_or(1.0f);
        max_runtime_sec_ = SharedState::get_typed<int>("config.climate.max_runtime").value_or(3600);
        
        ESP_LOGI(TAG, "Config loaded: setpoint=%.1f°C, hysteresis=%.1f°C, max_runtime=%ds",
                 setpoint_, hysteresis_, max_runtime_sec_);
    }
    
    void on_config_change(const std::string& key, const nlohmann::json& value) {
        ESP_LOGI(TAG, "Config changed: %s", key.c_str());
        
        if (key == "config.climate.setpoint") {
            setpoint_ = value.get<float>();
            ESP_LOGI(TAG, "Setpoint updated to %.1f°C", setpoint_);
        } else if (key == "config.climate.hysteresis") {
            hysteresis_ = value.get<float>();
            ESP_LOGI(TAG, "Hysteresis updated to %.1f°C", hysteresis_);
        }
        
        // Негайне застосування нової конфігурації
        apply_config_changes();
    }
    
    float setpoint_, hysteresis_;
    int max_runtime_sec_;
    SubscriptionHandle config_subscription_;
};
```

## Конфігурація та оптимізація

### Kconfig параметри

```kconfig
# У файлі Kconfig.projbuild
menu "SharedState Configuration"
    
    config SHARED_STATE_MAX_ENTRIES
        int "Maximum number of key-value pairs"
        default 64
        range 16 256
        help
            Maximum entries in SharedState storage.
            Larger values use more RAM but support more data.
            
    config SHARED_STATE_MAX_SUBSCRIPTIONS
        int "Maximum number of subscriptions"
        default 32
        range 8 64
        help
            Maximum number of active subscriptions.
            
    config SHARED_STATE_ENABLE_DIAGNOSTICS
        bool "Enable detailed diagnostics"
        default y
        help
            Include detailed performance counters and diagnostics.
            Disable to save a few bytes of RAM.
            
endmenu
```

### Оптимізація пам'яті

```cpp
// Конфігурація для різних сценаріїв використання

// Мала система (обмежена пам'ять) - 16 записів
#define CONFIG_SHARED_STATE_MAX_ENTRIES 16

// Стандартна система - 64 записи (за замовчуванням)
#define CONFIG_SHARED_STATE_MAX_ENTRIES 64

// Велика система (багато датчиків) - 128 записів
#define CONFIG_SHARED_STATE_MAX_ENTRIES 128

// Розрахунок використання пам'яті:
// Кожен запис: 32B (key) + ~20B (JSON overhead) + змінний розмір даних
// Для 64 записів: ~3-4KB статичної пам'яті
```

### Рекомендації з продуктивності

```cpp
// ✅ ДОБРІ практики:

// 1. Використовуйте helper методи для простих типів
SharedState::set_typed("sensor.temp", 25.5f);  // Простіше за JSON
auto temp = SharedState::get_typed<float>("sensor.temp");

// 2. Групуйте зв'язані дані в один JSON об'єкт
nlohmann::json sensor_data = {
    {"temperature", 25.5},
    {"humidity", 60.0},
    {"pressure", 1013.25}
};
SharedState::set("sensor.chamber.readings", sensor_data);

// 3. Використовуйте паттерни підписки для ефективності
SharedState::subscribe("sensor.*", callback);  // Один callback для всіх датчиків

// 4. Перевіряйте has_changed() перед costly операціями
static uint64_t last_ui_update = 0;
if (SharedState::has_changed("sensor.*", last_ui_update)) {
    update_display();  // Оновлюємо тільки при необхідності
    last_ui_update = esp_timer_get_time();
}

// ❌ УНИКАЙТЕ:

// 1. Частого створення/видалення ключів (фрагментація)
// SharedState::remove(key);  // Рідко потрібно

// 2. Дуже великих JSON об'єктів (обмежена пам'ять ESP32)
// nlohmann::json huge_data = { ... 1000 полів ... };

// 3. Довгих callback'ів (блокують інші підписки)
// SharedState::subscribe("*", long_running_callback);

// 4. Викликів з ISR
// void IRAM_ATTR gpio_isr() {
//     SharedState::set(...);  // ❌ НЕ МОЖНА!
// }
```

## Діагностика та відлагодження

### Логування та моніторинг

```cpp
// Увімкнути детальне логування (тільки для відлагодження)
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

// Моніторинг всіх змін (для діагностики)
void enable_debug_monitoring() {
    SharedState::subscribe("*", [](const std::string& key, const nlohmann::json& value) {
        ESP_LOGD(TAG, "SharedState change: %s = %s", 
                 key.c_str(), value.dump().c_str());
    });
}

// Періодичний health check
void periodic_health_check() {
    auto stats = SharedState::get_stats();
    
    ESP_LOGI(TAG, "SharedState Health Report:");
    ESP_LOGI(TAG, "  Entries: %zu/%zu (%.1f%% full)", 
             stats.used, stats.capacity, 
             (float)stats.used / stats.capacity * 100.0f);
    ESP_LOGI(TAG, "  Peak usage: %zu entries", stats.peak_used);
    ESP_LOGI(TAG, "  Operations: %zu sets, %zu gets", 
             stats.total_sets, stats.total_gets);
    ESP_LOGI(TAG, "  Subscriptions: %zu active", stats.subscriptions);
    
    // Попередження про потенційні проблеми
    if (stats.used > stats.capacity * 0.8) {
        ESP_LOGW(TAG, "SharedState usage high - consider increasing capacity");
    }
    
    if (stats.subscriptions > 20) {
        ESP_LOGW(TAG, "Many subscriptions - check for memory leaks");
    }
}
```

### Типові проблеми та рішення

1. **Переповнення сховища**
   - Симптом: ESP_ERR_NO_MEM при set()
   - Рішення: Збільшити CONFIG_SHARED_STATE_MAX_ENTRIES

2. **Повільні callback'и**
   - Симптом: Затримки в системі
   - Рішення: Оптимізувати callback функції, уникати блокуючих операцій

3. **Витік пам'яті JSON**
   - Симптом: Зменшення вільної пам'яті
   - Рішення: Перевірити, що JSON об'єкти не зростають безконтрольно

4. **Втрачені підписки**
   - Симптом: Callback'и не викликаються
   - Рішення: Зберігати SubscriptionHandle та правильно відписуватися

---

## Висновок

SharedState забезпечує надійний та ефективний механізм обміну даними для промислових холодильних систем ModuChill. Його JSON-орієнтований підхід дозволяє гнучко працювати зі складними даними датчиків та актуаторів, а повна інтеграція з ESP-IDF гарантує надійність в промислових умовах.

**Ключові переваги для промислового використання:**
- ✅ Статична алокація пам'яті для передбачуваної роботи
- ✅ Повна діагностика та моніторинг
- ✅ ESP-IDF інтеграція з надійною обробкою помилок
- ✅ Thread-safe операції для багатозадачності
- ✅ Гнучкість JSON для складних структур даних
- ✅ Атомарні операції для критичних лічильників

---

*Документ оновлено відповідно до реальної реалізації: 2025-06-29*

