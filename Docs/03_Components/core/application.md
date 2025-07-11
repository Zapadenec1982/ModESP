# Application - Центральний координатор життєвого циклу ModuChill

## Огляд і призначення

Application є центральним компонентом життєвого циклу системи ModuChill для промислових холодильних установок. Забезпечує детерміністичний запуск, стабільну роботу в реальному часі та грациозне завершення роботи з повною підтримкою багатоядерної архітектури ESP32-S3.

## Ключові можливості

* **Багатоядерна архітектура**: Оптимізована для ESP32-S3 з розподілом навантаження між ядрами
* **Lifecycle Management**: Керування станами BOOT → INIT → RUNNING → ERROR/SHUTDOWN
* **Фіксована частота виконання**: 100Hz головний цикл для передбачуваного таймінгу
* **Моніторинг ресурсів**: Відстеження heap, CPU, stack usage в реальному часі
* **Error handling**: Класифікація помилок, стратегії відновлення, degraded operation
* **Health monitoring**: Системна діагностика та reporting
* **Функціональне тестування**: Автоматичні тести компонентів при запуску
* **Performance tracking**: Детальні метрики продуктивності для оптимізації

## Архітектурні принципи

* **Детерміністична ініціалізація**: Фіксований порядок ініціалізації з урахуванням залежностей
* **Фіксована частота виконання**: 100Hz головний цикл для передбачуваного таймінгу
* **Multicore optimization**: Core 0 для бізнес-логіки, Core 1 для датчиків
* **Graceful degradation**: Система продовжує роботу без некритичних компонентів
* **Resource bounded**: Всі операції мають обмеження за часом/пам'яттю
* **Промислова надійність**: Відстеження помилок, перезапуск при критичних ситуаціях

## Використання пам'яті

| Компонент | Статична пам'ять |
|-----------|-------------------|
| Application state | 128-256B |
| Main task stack | 12KB (збільшено для JSON parsing) |
| Sensor task stack | 4KB |
| Performance metrics | ~100B |
| **Загалом** | **~16KB RAM** |

## Багатоядерна архітектура

```
ESP32-S3 Dual Core Architecture

Core 0 (Protocol CPU)                    Core 1 (Application CPU)
┌─────────────────────────┐             ┌─────────────────────────┐
│     Main Application    │             │     Sensor Task         │
│                         │             │                         │
│  • Business Logic       │             │  • SensorModule         │
│  • Climate Control      │             │  • Data Collection      │
│  • Actuator Control     │             │  • 10Hz Updates         │
│  • Event Processing     │             │  • Real-time Reading    │
│  • UI/Network          │             │                         │
│  • 100Hz Main Loop     │             │                         │
│                         │             │                         │
│  ┌─────────────────────┐│             │ ┌─────────────────────┐ │
│  │   Module Updates    ││             │ │  Sensor Drivers     │ │
│  │   (8ms budget)      ││◄────────────┤ │  DS18B20, NTC, etc. │ │
│  └─────────────────────┘│             │ └─────────────────────┘ │
│                         │             │                         │
│  ┌─────────────────────┐│             │ ┌─────────────────────┐ │
│  │  Event Processing   ││             │ │   Shared State      │ │
│  │   (2ms budget)      ││◄────────────┤ │   Publishing        │ │
│  └─────────────────────┘│             │ └─────────────────────┘ │
└─────────────────────────┘             └─────────────────────────┘
             │                                       │
             └───────────────────┬───────────────────┘
                                 │
                    ┌─────────────────────┐
                    │    Shared State     │
                    │   Event Bus        │
                    │   (Sync Primitives) │
                    └─────────────────────┘
```

### Розподіл навантаження

**Core 0 (Main Task):**
- Бізнес-логіка модулів (Climate Control, Defrost Control)
- Actuator управління
- Event Bus processing
- UI/Network communication
- Health monitoring
- Configuration management

**Core 1 (Sensor Task):**
- Sensor data collection
- Real-time sensor readings
- Data validation та filtering
- SharedState publishing


## State Machine & Lifecycle

### Системні стани

```cpp
enum class State {
    BOOT,       // Hardware initialization (0-10ms)
    INIT,       // Service initialization (10-500ms)
    RUNNING,    // Normal operation
    ERROR,      // Recovery mode
    SHUTDOWN    // Graceful shutdown
};
```

**BOOT (0-10ms)**
```
├── Hardware initialization (NVS, GPIO, UART)
├── System clock configuration
├── Watchdog setup
├── MAC address reading
└── Boot diagnostics
```

**INIT (10-500ms)**
```
├── Core services initialization
│   ├── EventBus::init()
│   ├── SharedState::init()
│   ├── ConfigManager::init()
│   └── ModuleManager::init()
├── HAL layer initialization (ESPhal::init())
├── Built-in sensor drivers initialization
├── Module registration (ModuleRegistry::register_all_modules())
├── Initial configuration loading (stack-safe)
├── Module configuration and initialization
├── Full configuration loading from storage
├── Functional tests execution
└── System startup event publishing
```

**RUNNING (Normal operation)**
```
Main Loop @ 100Hz (Core 0):
├── Module updates (8ms budget) - except sensors
├── Event processing (2ms budget)
├── Health monitoring (1Hz)
├── Performance metrics collection
├── CPU usage calculation
└── Watchdog feeding

Sensor Task @ 10Hz (Core 1):
├── Sensor data collection via SensorModule
├── SharedState publishing
├── Performance monitoring
└── Error reporting
```

**ERROR (Recovery mode)**
```
├── Error classification (WARNING/ERROR/CRITICAL/FATAL)
├── Recovery attempts based on severity
├── Component disabling for non-critical errors
├── Degraded operation mode
├── Error reporting via EventBus
├── Automatic restart for CRITICAL/FATAL errors
└── Error count tracking
```

**SHUTDOWN (Graceful stop)**
```
├── Stop accepting new operations
├── Terminate sensor task (Core 1)
├── Complete pending work
├── Save persistent state via ConfigManager::save()
├── Module shutdown (reverse priority order)
├── Resource cleanup
└── System halt or restart
```

### Переходи станів

* **BOOT → INIT**: Автоматично після готовності hardware
* **INIT → RUNNING**: Всі критичні сервіси ініціалізовані + тести пройдені
* **RUNNING → ERROR**: Критична помилка компонента
* **ERROR → RUNNING**: Успішне відновлення
* **ANY → SHUTDOWN**: Запит на завершення роботи
* **ERROR → BOOT**: Перезапуск після фатальної помилки (через esp_restart())

## Ініціалізація (Критичний порядок)

```
Порядок ініціалізації (НЕДОТРИМАННЯ = UNDEFINED BEHAVIOR):

1. Hardware Layer
   ├── NVS Flash initialization
   ├── GPIO, UART setup
   └── System clocks

2. Core Services (без залежностей)
   ├── EventBus::init()
   ├── SharedState::init()
   ├── ConfigManager::init()
   └── ModuleManager::init()

3. Hardware Abstraction
   └── ESPhal::init() (залежить від Config)

4. Driver Registration
   ├── initialize_builtin_sensor_drivers()
   └── ModuleRegistry::register_all_modules()

5. Configuration Management
   ├── ConfigManager::load_initial_config() (stack-safe)
   ├── ModuleManager::configure_all()
   └── ConfigManager::load() (повна конфігурація)

6. Module Initialization
   └── ModuleManager::init_all() (за пріоритетами)

7. System Validation
   ├── CoreTests::run_all_tests()
   └── EventBus::publish("system.started")
```

## API Reference

### Lifecycle Control

```cpp
/**
 * @brief Ініціалізація додатку
 * 
 * Виконує одноразову ініціалізацію всіх компонентів системи
 * у визначеному порядку. Включає функціональні тести.
 * 
 * @return ESP_OK при успіху, код помилки інакше
 */
esp_err_t init();

/**
 * @brief Запуск основного циклу додатку
 * 
 * Запускає головний цикл на 100Hz та sensor task на Core 1.
 * Ця функція блокується назавжди.
 * 
 * Структура головного циклу:
 * - Оновлення модулів (8ms budget)
 * - Обробка подій (2ms budget)
 * - Health monitoring (1Hz)
 * - Watchdog feeding
 */
[[noreturn]] void run();

/**
 * @brief Запит грациозного завершення
 * 
 * Ініціює послідовність завершення:
 * 1. Зупинка нових операцій
 * 2. Завершення поточної роботи
 * 3. Збереження стану
 * 4. Завершення модулів у зворотному порядку
 * 5. Очищення ресурсів
 */
void shutdown();

/**
 * @brief Запит перезапуску системи
 * 
 * @param delay_ms Затримка перед перезапуском (за замовчуванням 1000ms)
 */
void restart(uint32_t delay_ms = 1000);
```

### State Management

```cpp
/**
 * @brief Отримання поточного стану додатку
 * @return Поточний стан
 */
State get_state();

/**
 * @brief Перевірка чи додаток працює
 * @return true якщо в стані RUNNING
 */
bool is_running();
```

### Error Handling

```cpp
/**
 * @brief Рівні серйозності помилок
 */
enum class ErrorSeverity {
    WARNING,    // Некритична, тільки логується
    ERROR,      // Можлива деградована робота
    CRITICAL,   // Потрібен перезапуск системи
    FATAL       // Негайний перезапуск
};

/**
 * @brief Звітування про помилку від компонента
 * 
 * @param component Ім'я компонента (наприклад, "climate_control")
 * @param error Код помилки ESP-IDF
 * @param severity Рівень серйозності помилки
 * @param message Опціональне повідомлення про помилку
 */
void report_error(const char* component, esp_err_t error, 
                 ErrorSeverity severity, const char* message = nullptr);
```

### Health Monitoring

```cpp
/**
 * @brief Виконання системної перевірки здоров'я
 * 
 * Перевіряє:
 * - Вільну пам'ять heap
 * - Використання стеку
 * - Health scores модулів
 * - Критичні системні ресурси
 * 
 * @return true якщо система здорова
 */
bool check_health();
```

### System Metrics

```cpp
/**
 * @brief Отримання часу роботи системи в мілісекундах
 * @return Мілісекунди з моменту загрузки
 */
uint32_t get_uptime_ms();

/**
 * @brief Отримання вільної пам'яті heap
 * @return Вільна пам'ять у байтах
 */
size_t get_free_heap();

/**
 * @brief Отримання мінімальної вільної пам'яті з моменту загрузки
 * @return Мінімальна вільна пам'ять у байтах
 */
size_t get_min_free_heap();

/**
 * @brief Отримання використання CPU у відсотках
 * @return Використання CPU 0-100%
 */
uint8_t get_cpu_usage();

/**
 * @brief Отримання high water mark стеку головної задачі
 * @return Залишилося байт стеку
 */
size_t get_stack_high_water_mark();

/**
 * @brief Отримання інстансу ESPhal
 * @return Посилання на інстанс ESPhal
 */
ESPhal& get_hal();
```


## Архітектура головного циклу

### Структура циклу (10ms @ 100Hz)

```cpp
// Спрощена схема головного циклу
while (current_state == State::RUNNING) {
    uint32_t cycle_start = esp_timer_get_time();
    
    // 1. Оновлення модулів (8ms budget) - крім сенсорів
    ModuleManager::tick_all_except_sensors(MODULE_UPDATE_BUDGET_MS);
    
    // 2. Обробка подій (2ms budget)
    EventBus::process(EVENT_PROCESS_BUDGET_MS);
    
    // 3. Health check (1Hz)
    if (now_ms - last_health_check_ms >= HEALTH_CHECK_PERIOD_MS) {
        check_health();
        log_performance_metrics();
        last_health_check_ms = now_ms;
    }
    
    // 4. Оновлення метрик продуктивності
    update_cycle_metrics(cycle_start);
    
    // 5. Попередження про перевищення часу
    uint32_t cycle_time = esp_timer_get_time() - cycle_start;
    if (cycle_time > MAIN_LOOP_PERIOD_MS * 1000) {
        ESP_LOGW(TAG, "Cycle overrun: %lu us", cycle_time);
    }
    
    // 6. Фіксована частота виконання
    vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(MAIN_LOOP_PERIOD_MS));
}
```

### Часові бюджети

| Операція | Час | Частота | Опис |
|----------|-----|---------|------|
| **Module Updates** | 8ms | 100Hz | Оновлення бізнес-логіки модулів |
| **Event Processing** | 2ms | 100Hz | Обробка черги подій |
| **Health Check** | ~1ms | 1Hz | Системна діагностика |
| **Performance Logging** | ~0.5ms | 1Hz | Збір та логування метрик |
| **Cycle Management** | ~0.5ms | 100Hz | Управління циклом та таймінгом |
| **РЕЗЕРВ** | -2ms | - | Залишається для непередбачених затримок |

### Sensor Task (Core 1)

```cpp
// Спрощена схема sensor task
static void sensor_task(void* pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t sensor_period = pdMS_TO_TICKS(100); // 10Hz
    
    while (current_state == State::RUNNING) {
        uint64_t start_time_us = esp_timer_get_time();
        
        // Знайти та оновити SensorModule на Core 1
        BaseModule* sensor_module = ModuleManager::find_module("SensorModule");
        if (sensor_module) {
            sensor_module->update();
        }
        
        // Метрики продуктивності sensor task
        update_sensor_task_metrics(start_time_us);
        
        vTaskDelayUntil(&last_wake_time, sensor_period);
    }
    
    vTaskDelete(nullptr);
}
```

## Приклади використання для промислового обладнання

### Ініціалізація та запуск системи

```cpp
// main.c - Точка входу системи
void app_main(void) {
    ESP_LOGI("Main", "ModuChill Industrial System starting...");
    
    // Запуск C++ додатку через wrapper
    launch_application();
}

// application_wrapper.cpp - C++ wrapper
extern "C" void launch_application() {
    // Ініціалізація додатку
    esp_err_t ret = Application::init();
    if (ret != ESP_OK) {
        ESP_LOGE("App", "Failed to initialize: %s", esp_err_to_name(ret));
        return;
    }
    
    // Запуск головного циклу (блокується назавжди)
    Application::run();
}
```

### Перевірка стану системи

```cpp
// Модуль для моніторингу стану системи
class SystemMonitor {
public:
    void init() {
        // Підписка на системні події
        EventBus::subscribe("system.*", [this](const Event& e) {
            handle_system_event(e);
        });
        
        // Підписка на помилки
        EventBus::subscribe("system.error", [this](const Event& e) {
            handle_error_event(e);
        });
    }
    
    void update() {
        // Перевірка критичних параметрів
        check_critical_parameters();
        
        // Публікація статусу системи
        publish_system_status();
    }
    
private:
    void check_critical_parameters() {
        // Перевірка пам'яті
        size_t free_heap = Application::get_free_heap();
        if (free_heap < 10240) { // < 10KB
            Application::report_error("SystemMonitor", ESP_ERR_NO_MEM, 
                                    Application::ErrorSeverity::ERROR,
                                    "Low heap memory");
        }
        
        // Перевірка CPU
        uint8_t cpu_usage = Application::get_cpu_usage();
        if (cpu_usage > 95) {
            Application::report_error("SystemMonitor", ESP_ERR_TIMEOUT,
                                    Application::ErrorSeverity::CRITICAL,
                                    "High CPU usage");
        }
        
        // Перевірка стеку
        size_t stack_remaining = Application::get_stack_high_water_mark();
        if (stack_remaining < 1024) { // < 1KB
            Application::report_error("SystemMonitor", ESP_ERR_NO_MEM,
                                    Application::ErrorSeverity::CRITICAL,
                                    "Low stack space");
        }
    }
    
    void publish_system_status() {
        nlohmann::json status = {
            {"state", static_cast<int>(Application::get_state())},
            {"uptime_ms", Application::get_uptime_ms()},
            {"free_heap", Application::get_free_heap()},
            {"min_free_heap", Application::get_min_free_heap()},
            {"cpu_usage", Application::get_cpu_usage()},
            {"stack_remaining", Application::get_stack_high_water_mark()},
            {"is_healthy", Application::check_health()}
        };
        
        SharedState::set("system.status", status);
    }
    
    void handle_system_event(const Event& e) {
        if (e.type == "system.started") {
            ESP_LOGI(TAG, "System started successfully");
            auto data = e.data;
            ESP_LOGI(TAG, "Tests: %d/%d passed", 
                     data["tests_passed"].get<int>(),
                     data["tests_total"].get<int>());
        }
    }
    
    void handle_error_event(const Event& e) {
        auto data = e.data;
        std::string component = data["component"];
        int error_code = data["error_code"];
        int severity = data["severity"];
        std::string message = data["message"];
        
        ESP_LOGE(TAG, "System error from %s: 0x%x (severity: %d) - %s",
                 component.c_str(), error_code, severity, message.c_str());
        
        // Критичні помилки потребують негайної уваги
        if (severity >= static_cast<int>(Application::ErrorSeverity::CRITICAL)) {
            // Відправити аварійний сигнал
            EventBus::publish("alarm.critical", {
                {"source", "SystemMonitor"},
                {"reason", "Critical system error"},
                {"component", component},
                {"error_code", error_code}
            }, EventBus::Priority::CRITICAL);
        }
    }
};
```

### Error Handling для промислового обладнання

```cpp
// Приклад error handling в модулі керування компресором
class CompressorControl {
public:
    void update() {
        try {
            // Спроба керування компресором
            control_compressor();
        } catch (const std::exception& e) {
            // Перехоплення виключень
            Application::report_error("CompressorControl", ESP_FAIL,
                                    Application::ErrorSeverity::ERROR,
                                    e.what());
        }
    }
    
private:
    void control_compressor() {
        // Перевірка попередніх умов
        if (!check_safety_conditions()) {
            Application::report_error("CompressorControl", ESP_ERR_INVALID_STATE,
                                    Application::ErrorSeverity::WARNING,
                                    "Safety conditions not met");
            return;
        }
        
        // Спроба вмикання компресора
        esp_err_t err = turn_on_compressor();
        if (err != ESP_OK) {
            // Визначення серйозності помилки
            Application::ErrorSeverity severity;
            if (err == ESP_ERR_TIMEOUT) {
                severity = Application::ErrorSeverity::ERROR;
            } else if (err == ESP_ERR_NO_MEM) {
                severity = Application::ErrorSeverity::CRITICAL;
            } else {
                severity = Application::ErrorSeverity::WARNING;
            }
            
            Application::report_error("CompressorControl", err, severity,
                                    "Failed to turn on compressor");
        }
    }
    
    bool check_safety_conditions() {
        // Перевірка температури
        auto temp = SharedState::get_typed<float>("sensor.chamber.temperature");
        if (temp && temp.value() > 10.0f) {
            return false; // Занадто тепло для роботи компресора
        }
        
        // Перевірка тиску
        auto pressure = SharedState::get_typed<float>("sensor.pressure");
        if (pressure && pressure.value() > 25.0f) {
            return false; // Високий тиск
        }
        
        return true;
    }
    
    esp_err_t turn_on_compressor() {
        // Реалізація увімкнення компресора
        // Повертає ESP_OK при успіху або код помилки
        return ESP_OK;
    }
};
```

### Graceful Shutdown для промислових систем

```cpp
// Обробка сигналів завершення роботи
class GracefulShutdownHandler {
public:
    void init() {
        // Підписка на системні події завершення
        EventBus::subscribe("system.shutdown_request", [this](const Event& e) {
            handle_shutdown_request(e);
        });
        
        // Підписка на критичні помилки
        EventBus::subscribe("alarm.critical", [this](const Event& e) {
            handle_critical_alarm(e);
        });
    }
    
private:
    void handle_shutdown_request(const Event& e) {
        ESP_LOGI(TAG, "Graceful shutdown requested");
        
        // Зупинити нові операції
        stop_new_operations();
        
        // Завершити поточні критичні процеси
        complete_critical_processes();
        
        // Зберегти важливі дані
        save_operational_data();
        
        // Ініціювати shutdown
        Application::shutdown();
    }
    
    void handle_critical_alarm(const Event& e) {
        ESP_LOGE(TAG, "Critical alarm received, initiating emergency shutdown");
        
        // Аварійне завершення
        emergency_shutdown();
        
        // Перезапуск через 5 секунд
        Application::restart(5000);
    }
    
    void stop_new_operations() {
        // Відключити прийом нових команд
        SharedState::set("system.accepting_commands", false);
        
        // Зупинити планувальник задач
        SharedState::set("system.scheduler_enabled", false);
        
        ESP_LOGI(TAG, "New operations stopped");
    }
    
    void complete_critical_processes() {
        // Дочекатися завершення критичних процесів
        // наприклад, запису даних, завершення циклу розморожування
        
        ESP_LOGI(TAG, "Waiting for critical processes to complete...");
        
        // Максимум 30 секунд очікування
        int timeout_sec = 30;
        while (timeout_sec-- > 0) {
            if (all_critical_processes_complete()) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        
        ESP_LOGI(TAG, "Critical processes completed or timed out");
    }
    
    void save_operational_data() {
        // Зберегти поточні операційні дані
        nlohmann::json operational_data = {
            {"shutdown_time", esp_timer_get_time()},
            {"uptime_ms", Application::get_uptime_ms()},
            {"last_temperatures", get_last_temperatures()},
            {"compressor_hours", get_compressor_operating_hours()},
            {"error_count", get_total_error_count()}
        };
        
        SharedState::set("system.last_operational_data", operational_data);
        
        // Зберегти конфігурацію
        ConfigManager::save();
        
        ESP_LOGI(TAG, "Operational data saved");
    }
    
    void emergency_shutdown() {
        // Негайне відключення всіх актуаторів
        SharedState::set("command.actuator.compressor", false);
        SharedState::set("command.actuator.fan", false);
        SharedState::set("command.actuator.heater", false);
        
        // Аварійне збереження критичних даних
        save_emergency_data();
        
        ESP_LOGE(TAG, "Emergency shutdown completed");
    }
    
    bool all_critical_processes_complete() {
        // Перевірити чи всі критичні процеси завершені
        return true; // Спрощена реалізація
    }
    
    nlohmann::json get_last_temperatures() {
        // Отримати останні показання температури
        return nlohmann::json::array();
    }
    
    uint32_t get_compressor_operating_hours() {
        // Отримати години роботи компресора
        return 0;
    }
    
    uint32_t get_total_error_count() {
        // Отримати загальну кількість помилок
        return 0;
    }
    
    void save_emergency_data() {
        // Зберегти мінімально необхідні дані при аварійному завершенні
    }
};
```


## Performance Tuning та Оптимізація

### Метрики продуктивності

Application автоматично збирає детальні метрики продуктивності:

```cpp
// Метрики головного циклу (Core 0)
struct MainLoopMetrics {
    uint32_t cycle_count;              // Загальна кількість циклів
    uint32_t min_cycle_time_us;        // Мінімальний час циклу
    uint32_t max_cycle_time_us;        // Максимальний час циклу
    uint32_t total_cycle_time_us;      // Загальний час всіх циклів
    uint32_t overrun_count;            // Кількість перевищень часу
};

// Метрики sensor task (Core 1)
struct SensorTaskMetrics {
    uint32_t sensor_cycle_count;       // Кількість циклів sensors
    uint64_t sensor_total_time_us;     // Загальний час sensor task
    uint32_t sensor_max_time_us;       // Максимальний час sensor task
};

// Приклад отримання метрик
void log_performance_metrics() {
    uint32_t avg_cycle_us = cycle_count > 0 ? 
        (uint32_t)(total_cycle_time_us / cycle_count) : 0;
    
    ESP_LOGI(TAG, "Performance: CPU: %u%%, Cycle: %lu/%lu/%lu μs (min/avg/max), Free heap: %zu KB",
             get_cpu_usage(),
             min_cycle_time_us,
             avg_cycle_us,
             max_cycle_time_us,
             get_free_heap() / 1024);
}
```

### Multicore Optimization Best Practices

**Розподіл модулів між ядрами:**

```cpp
// Core 0 (Main Task) - Оптимальні модулі:
class CoreZeroModules {
    // ✅ Модулі з частими комунікаціями
    ClimateControlModule climate_control;    // Часті рішення та команди
    DefrostControlModule defrost_control;    // Логіка розморожування
    UIModule ui_module;                      // Інтерфейс користувача
    NetworkModule network_module;            // WiFi, HTTP, MQTT
    
    // ✅ Модулі з event-driven логікою
    AlarmModule alarm_module;                // Обробка аварій
    LoggingModule logging_module;            // Логування подій
    
    // ✅ Модулі з нечастими оновленнями
    RTCModule rtc_module;                    // Час та календар
    ConfigModule config_module;              // Управління конфігурацією
};

// Core 1 (Sensor Task) - Оптимальні модулі:
class CoreOneModules {
    // ✅ Модулі з real-time вимогами
    SensorModule sensor_module;             // Збір даних датчиків
    
    // ✅ Модулі з регулярними I/O операціями
    DataLoggerModule data_logger;           // Збереження даних
    
    // 🚫 УНИКАТИ на Core 1:
    // - Модулі з networking (WiFi conflicts)
    // - Модулі з великим використанням CPU
    // - Модулі з непередбачуваним часом виконання
};
```

### Налаштування часових бюджетів

```cpp
// Конфігурація таймінгу для різних навантажень
class TimingConfiguration {
public:
    // Стандартна конфігурація (64 датчика, 16 актуаторів)
    static constexpr uint32_t STANDARD_CONFIG = {
        .main_loop_period_ms = 10,        // 100Hz
        .module_update_budget_ms = 8,     // 8ms для модулів
        .event_process_budget_ms = 2,     // 2ms для подій
        .sensor_task_period_ms = 100,     // 10Hz для датчиків
    };
    
    // Високе навантаження (128 датчиків, 32 актуатори)
    static constexpr uint32_t HIGH_LOAD_CONFIG = {
        .main_loop_period_ms = 20,        // 50Hz (більше часу)
        .module_update_budget_ms = 15,    // 15ms для модулів
        .event_process_budget_ms = 4,     // 4ms для подій
        .sensor_task_period_ms = 50,      // 20Hz для датчиків
    };
    
    // Низьке навантаження (16 датчиків, 8 актуаторів)
    static constexpr uint32_t LOW_LOAD_CONFIG = {
        .main_loop_period_ms = 5,         // 200Hz (більша точність)
        .module_update_budget_ms = 3,     // 3ms для модулів
        .event_process_budget_ms = 1,     // 1ms для подій
        .sensor_task_period_ms = 200,     // 5Hz для датчиків
    };
};
```

### Оптимізація пам'яті

```cpp
// Рекомендації з оптимізації пам'яті
class MemoryOptimization {
public:
    static void configure_for_device_type() {
        // ESP32-S3 з 8MB PSRAM
        if (esp_psram_is_initialized()) {
            configure_large_memory_setup();
        } else {
            configure_minimal_memory_setup();
        }
    }
    
private:
    static void configure_large_memory_setup() {
        // Більші буфери для кращої продуктивності
        CONFIG_SHARED_STATE_MAX_ENTRIES = 128;
        CONFIG_EVENT_BUS_QUEUE_SIZE = 64;
        CONFIG_MODULE_MAX_COUNT = 32;
        
        ESP_LOGI(TAG, "Configured for large memory setup (8MB PSRAM)");
    }
    
    static void configure_minimal_memory_setup() {
        // Мінімальні буфери для економії пам'яті
        CONFIG_SHARED_STATE_MAX_ENTRIES = 32;
        CONFIG_EVENT_BUS_QUEUE_SIZE = 16;
        CONFIG_MODULE_MAX_COUNT = 16;
        
        ESP_LOGI(TAG, "Configured for minimal memory setup (no PSRAM)");
    }
};
```

### Monitoring та Alerting

```cpp
// Система моніторингу продуктивності
class PerformanceMonitor {
public:
    void update() {
        monitor_cycle_timing();
        monitor_memory_usage();
        monitor_task_performance();
        check_performance_alerts();
    }
    
private:
    void monitor_cycle_timing() {
        uint32_t avg_cycle_us = get_average_cycle_time();
        
        // Попередження про повільні цикли
        if (avg_cycle_us > 8000) { // > 8ms
            ESP_LOGW(TAG, "Slow cycles detected: %lu μs average", avg_cycle_us);
            
            SharedState::increment("metrics.performance.slow_cycles", 1.0);
            
            // Автоматична оптимізація
            if (avg_cycle_us > 12000) { // > 12ms
                suggest_performance_tuning();
            }
        }
    }
    
    void monitor_memory_usage() {
        size_t free_heap = Application::get_free_heap();
        size_t min_free_heap = Application::get_min_free_heap();
        
        // Попередження про низьку пам'ять
        if (free_heap < 20480) { // < 20KB
            ESP_LOGW(TAG, "Low memory: %zu KB free, %zu KB minimum",
                     free_heap / 1024, min_free_heap / 1024);
            
            SharedState::increment("metrics.performance.low_memory_warnings", 1.0);
        }
        
        // Публікація метрик пам'яті
        SharedState::set_typed("metrics.memory.free_heap_kb", (int)(free_heap / 1024));
        SharedState::set_typed("metrics.memory.min_free_heap_kb", (int)(min_free_heap / 1024));
    }
    
    void monitor_task_performance() {
        // Моніторинг використання стеку
        size_t stack_remaining = Application::get_stack_high_water_mark();
        
        if (stack_remaining < 2048) { // < 2KB
            ESP_LOGW(TAG, "Low stack space: %zu bytes remaining", stack_remaining);
            
            Application::report_error("PerformanceMonitor", ESP_ERR_NO_MEM,
                                    Application::ErrorSeverity::ERROR,
                                    "Low stack space detected");
        }
    }
    
    void check_performance_alerts() {
        // Перевірка критичних метрик продуктивності
        uint8_t cpu_usage = Application::get_cpu_usage();
        
        if (cpu_usage > 90) {
            ESP_LOGE(TAG, "Critical CPU usage: %u%%", cpu_usage);
            
            // Аварійний режим - зменшити частоту оновлень
            emergency_performance_mode();
        }
    }
    
    void suggest_performance_tuning() {
        ESP_LOGI(TAG, "Performance tuning suggestions:");
        ESP_LOGI(TAG, "  - Increase main loop period to 20ms");
        ESP_LOGI(TAG, "  - Reduce module update frequency");
        ESP_LOGI(TAG, "  - Check for blocking operations in modules");
        ESP_LOGI(TAG, "  - Consider disabling non-critical modules");
    }
    
    void emergency_performance_mode() {
        ESP_LOGW(TAG, "Entering emergency performance mode");
        
        // Зменшити частоту оновлень
        SharedState::set("system.emergency_mode", true);
        
        // Відключити некритичні модулі
        ModuleManager::disable_non_critical_modules();
        
        // Збільшити період головного циклу
        // (потребує перезапуску для застосування)
        SharedState::set("config.system.main_loop_period_ms", 20);
    }
    
    uint32_t get_average_cycle_time() {
        // Реалізація отримання середнього часу циклу
        return 5000; // Заглушка
    }
};
```

## Troubleshooting та Діагностика

### Типові проблеми та рішення

**1. Перевищення часу циклу (Cycle Overrun)**

```cpp
// Симптом: "Cycle overrun: XXXXX us"
// Причини та рішення:

void troubleshoot_cycle_overrun() {
    ESP_LOGI(TAG, "Troubleshooting cycle overrun...");
    
    // Перевірка 1: Модулі з довгим часом виконання
    auto module_times = ModuleManager::get_execution_times();
    for (const auto& [module_name, time_us] : module_times) {
        if (time_us > 2000) { // > 2ms
            ESP_LOGW(TAG, "Slow module detected: %s (%lu μs)", 
                     module_name.c_str(), time_us);
        }
    }
    
    // Перевірка 2: Блокуючі операції
    check_for_blocking_operations();
    
    // Перевірка 3: Переповнення черги подій
    size_t event_queue_size = EventBus::get_queue_size();
    if (event_queue_size > 10) {
        ESP_LOGW(TAG, "Event queue overloaded: %zu events pending", 
                 event_queue_size);
    }
    
    // Рішення: Автоматичне налаштування
    if (get_overrun_frequency() > 5) { // > 5% циклів
        ESP_LOGW(TAG, "Applying automatic performance tuning...");
        apply_automatic_tuning();
    }
}
```

**2. Нестача пам'яті (Memory Issues)**

```cpp
void troubleshoot_memory_issues() {
    size_t free_heap = Application::get_free_heap();
    size_t min_free_heap = Application::get_min_free_heap();
    
    ESP_LOGI(TAG, "Memory diagnostics:");
    ESP_LOGI(TAG, "  Current free: %zu KB", free_heap / 1024);
    ESP_LOGI(TAG, "  Minimum free: %zu KB", min_free_heap / 1024);
    ESP_LOGI(TAG, "  Stack remaining: %zu bytes", 
             Application::get_stack_high_water_mark());
    
    // Аналіз використання пам'яті компонентами
    auto sharedstate_stats = SharedState::get_stats();
    ESP_LOGI(TAG, "SharedState: %zu/%zu entries (%zu bytes each)",
             sharedstate_stats.used, sharedstate_stats.capacity,
             sizeof(SharedStateEntry));
    
    // Рекомендації
    if (free_heap < 30720) { // < 30KB
        ESP_LOGW(TAG, "Memory optimization recommendations:");
        ESP_LOGW(TAG, "  - Reduce SharedState max entries");
        ESP_LOGW(TAG, "  - Disable debug logging");
        ESP_LOGW(TAG, "  - Consider PSRAM usage");
    }
}
```

**3. Multicore синхронізація**

```cpp
void troubleshoot_multicore_issues() {
    // Перевірка балансу навантаження між ядрами
    uint32_t core0_cycles = get_core0_cycle_count();
    uint32_t core1_cycles = get_core1_cycle_count();
    
    float core_balance = (float)core1_cycles / core0_cycles;
    
    ESP_LOGI(TAG, "Core balance: Core0: %lu, Core1: %lu (ratio: %.2f)",
             core0_cycles, core1_cycles, core_balance);
    
    if (core_balance < 0.1 || core_balance > 2.0) {
        ESP_LOGW(TAG, "Core load imbalance detected");
        suggest_core_rebalancing();
    }
    
    // Перевірка конфліктів доступу до SharedState
    check_sharedstate_contention();
}
```

## Best Practices для промислового використання

### ✅ **Рекомендації**

1. **Завжди перевіряйте return codes**
   ```cpp
   esp_err_t err = Application::init();
   if (err != ESP_OK) {
       // Обробити помилку
   }
   ```

2. **Використовуйте error reporting**
   ```cpp
   Application::report_error("MyModule", ESP_ERR_TIMEOUT,
                           Application::ErrorSeverity::ERROR,
                           "Operation timeout");
   ```

3. **Моніторіть системні ресурси**
   ```cpp
   if (Application::get_free_heap() < CRITICAL_HEAP_THRESHOLD) {
       // Зменшити навантаження
   }
   ```

4. **Налаштуйте таймінги під ваше обладнання**
   ```cpp
   // Більше датчиків = більший період циклу
   #define MAIN_LOOP_PERIOD_MS (sensor_count > 64 ? 20 : 10)
   ```

### ❌ **Чого уникати**

1. **Блокуючі операції в модулях**
   ```cpp
   // ❌ НЕ РОБІТЬ ТАК
   void SlowModule::update() {
       vTaskDelay(pdMS_TO_TICKS(100)); // Блокує на 100ms!
   }
   ```

2. **Динамічна алокація в циклі**
   ```cpp
   // ❌ НЕ РОБІТЬ ТАК
   void BadModule::update() {
       char* buffer = malloc(1024); // Кожні 10ms!
       // ...
       free(buffer);
   }
   ```

3. **Довгі операції без таймаутів**
   ```cpp
   // ❌ НЕ РОБІТЬ ТАК
   while (!operation_complete()) {
       // Може заблокувати назавжди
   }
   ```

## Висновок

Application компонент забезпечує надійну та ефективну основу для промислових холодильних систем ModuChill. Його багатоядерна архітектура, детальний моніторинг та robust error handling роблять його ідеальним для критично важливих промислових застосувань.

### **Ключові переваги**

- ✅ **Детерміністична робота** - фіксовані таймінги для real-time систем
- ✅ **Багатоядерна оптимізація** - ефективне використання ESP32-S3
- ✅ **Повний моніторинг** - детальні метрики для діагностики
- ✅ **Robust error handling** - класифікація та автоматичне відновлення
- ✅ **Graceful degradation** - система продовжує роботу при частковому відмові
- ✅ **Промислова надійність** - перевірено в real-world умовах

### **Готовність до виробництва**

Application готовий для використання в промислових холодильних установках з підтримкою:
- Складних multi-zone конфігурацій
- Real-time моніторингу та контролю
- Автоматичної діагностики та відновлення
- Remote management та OTA updates
- Повної сумісності з існуючими промисловими протоколами

---

*Документація оновлена відповідно до реальної реалізації: 2025-06-29*

