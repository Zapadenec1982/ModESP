# EventBus - Асинхронна система подій

## Огляд

EventBus - це потокобезпечна система публікації-підписки для ModESP, яка забезпечує асинхронну комунікацію між модулями. Вона підтримує пріоритетність подій, шаблони підписки та фільтрацію.

## Основні можливості

- **Потокобезпечність**: Публікація з будь-якого потоку або ISR
- **Пріоритетність**: 4 рівні пріоритету для обробки подій
- **Шаблони підписки**: Підписка на конкретні події або групи
- **Фільтрація**: Можливість встановлення глобального фільтра
- **Статистика**: Детальна статистика роботи системи
- **Управління чергою**: Налаштовуваний розмір черги подій

## Архітектура

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│   Publisher 1   │     │   Publisher 2   │     │   Publisher N   │
└────────┬────────┘     └────────┬────────┘     └────────┬────────┘
         │                       │                       │
         └───────────────────────┴───────────────────────┘
                                 │
                         ┌───────▼────────┐
                         │  Event Queue   │
                         │  (Priority)    │
                         └───────┬────────┘
                                 │
                         ┌───────▼────────┐
                         │    Process()   │
                         │  (Main Task)   │
                         └───────┬────────┘
                                 │
         ┌───────────────────────┼───────────────────────┐
         │                       │                       │
┌────────▼────────┐     ┌────────▼────────┐     ┌────────▼────────┐
│  Subscriber 1   │     │  Subscriber 2   │     │  Subscriber N   │
└─────────────────┘     └─────────────────┘     └─────────────────┘
```

## API Reference

### Ініціалізація

```cpp
esp_err_t EventBus::init(size_t queue_size = 32);
```

Ініціалізує EventBus з вказаним розміром черги. Має бути викликана один раз під час запуску системи.

### Публікація подій

```cpp
esp_err_t EventBus::publish(
    const std::string& type, 
    const nlohmann::json& data = {},
    Priority priority = Priority::NORMAL
);
```

Публікує подію з вказаним типом та даними. Можливі пріоритети:
- `Priority::CRITICAL` - критичні події безпеки
- `Priority::HIGH` - важливі операційні події
- `Priority::NORMAL` - звичайні події (за замовчуванням)
- `Priority::LOW` - фонові події

### Публікація з ISR

```cpp
esp_err_t EventBus::publish_from_isr(
    const std::string& type,
    const nlohmann::json& data,
    Priority priority,
    BaseType_t* higher_priority_task_woken
);
```

Спеціальна версія для публікації з обробників переривань.

### Підписка на події

```cpp
SubscriptionHandle EventBus::subscribe(
    const std::string& pattern, 
    EventHandler handler
);
```

Підписується на події за шаблоном. Повертає handle для відписки.

**Шаблони підписки:**
- `""` або `"*"` - підписка на всі події
- `"sensor.*"` - підписка на всі події сенсорів
- `"sensor.temperature"` - підписка на конкретну подію
- `"*.updated"` - підписка на всі події оновлення

### Відписка

```cpp
esp_err_t EventBus::unsubscribe(SubscriptionHandle handle);
```

Відписується від подій за handle.

### Обробка подій

```cpp
size_t EventBus::process(uint32_t max_ms = 2);
```

Обробляє події з черги протягом вказаного часу (мс). Повертає кількість оброблених подій.
## Методи управління

### Управління чергою

```cpp
size_t EventBus::get_queue_size();         // Поточний розмір черги
bool EventBus::is_queue_full();           // Чи заповнена черга
void EventBus::clear();                   // Очистити всі події в черзі
```

### Призупинення обробки

```cpp
void EventBus::pause();                   // Призупинити обробку
void EventBus::resume();                  // Відновити обробку
bool EventBus::is_paused();              // Перевірити статус
```

### Статистика

```cpp
EventBus::Stats EventBus::get_stats();    // Отримати статистику
void EventBus::reset_stats();             // Скинути статистику
```

Структура статистики:
```cpp
struct Stats {
    size_t total_published;        // Всього опубліковано
    size_t total_processed;        // Всього оброблено
    size_t total_dropped;          // Втрачено (переповнення)
    size_t queue_size;             // Поточний розмір черги
    size_t max_queue_size;         // Максимальний розмір
    size_t total_subscriptions;    // Активні підписки
    uint32_t avg_process_time_us;  // Середній час обробки (мкс)
};
```
### Фільтрація подій

```cpp
using EventFilter = std::function<bool(const Event&)>;
void EventBus::set_filter(EventFilter filter);
void EventBus::clear_filter();
```

Дозволяє встановити глобальний фільтр для подій перед додаванням в чергу.

## Приклади використання

### Базове використання

```cpp
// Ініціалізація (один раз під час запуску)
EventBus::init(64);  // Черга на 64 події

// Підписка на події температури
auto handle = EventBus::subscribe("sensor.temperature", 
    [](const EventBus::Event& event) {
        float temp = event.data["value"];
        ESP_LOGI(TAG, "Temperature: %.1f°C", temp);
    }
);

// Публікація події
nlohmann::json data = {
    {"value", 23.5},
    {"unit", "celsius"},
    {"sensor_id", "temp_01"}
};
EventBus::publish("sensor.temperature", data);

// В головному циклі
void app_main() {
    while (true) {
        EventBus::process(5);  // Обробляти до 5мс
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```
### Використання пріоритетів

```cpp
// Критична подія - аварійна зупинка
EventBus::publish("system.emergency_stop", 
    {{"reason", "overtemperature"}}, 
    EventBus::Priority::CRITICAL
);

// Звичайна подія
EventBus::publish("sensor.humidity", 
    {{"value", 65.0}}, 
    EventBus::Priority::NORMAL
);

// Фонова подія
EventBus::publish("system.stats", 
    stats_data, 
    EventBus::Priority::LOW
);
```

### Підписка з шаблонами

```cpp
// Підписка на всі події сенсорів
EventBus::subscribe("sensor.*", [](const auto& event) {
    ESP_LOGI(TAG, "Sensor event: %s", event.type.c_str());
});

// Підписка на всі критичні події
EventBus::subscribe("", [](const auto& event) {
    if (event.priority == EventBus::Priority::CRITICAL) {
        ESP_LOGE(TAG, "CRITICAL: %s", event.type.c_str());
    }
});
```

### Публікація з ISR

```cpp
void IRAM_ATTR gpio_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    EventBus::publish_from_isr(
        "gpio.interrupt",
        {{"pin", 12}, {"level", 1}},
        EventBus::Priority::HIGH,
        &xHigherPriorityTaskWoken
    );
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```
### Використання фільтра

```cpp
// Встановити фільтр для обмеження частоти подій
EventBus::set_filter([](const EventBus::Event& event) {
    static uint32_t last_time = 0;
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Пропускати не більше 1 події на 100мс для sensor.*
    if (event.type.find("sensor.") == 0) {
        if (now - last_time < 100) {
            return false;  // Відхилити подію
        }
        last_time = now;
    }
    
    return true;  // Прийняти подію
});
```

## Інтеграція з системою

### Типова структура модуля

```cpp
class TemperatureSensor : public IModule {
private:
    EventBus::SubscriptionHandle config_handle;
    
public:
    esp_err_t init() override {
        // Підписка на події конфігурації
        config_handle = EventBus::subscribe("config.temperature_sensor",
            [this](const auto& event) {
                update_config(event.data);
            }
        );
        
        // Запуск вимірювань
        start_measurements();
        return ESP_OK;
    }
    
    esp_err_t deinit() override {
        EventBus::unsubscribe(config_handle);
        return ESP_OK;
    }
    
private:
    void publish_temperature(float temp) {
        EventBus::publish("sensor.temperature", {
            {"value", temp},
            {"timestamp", esp_timer_get_time()},
            {"sensor_id", get_id()}
        });
    }
};
```
## Найкращі практики

### 1. Іменування подій

Використовуйте ієрархічну структуру з точками:
- `module.action` - основний формат
- `sensor.temperature.updated` - детальніший формат
- `system.emergency.stop` - критичні події

### 2. Структура даних

Завжди включайте:
- `timestamp` - час події
- `source` або `id` - джерело події
- Релевантні дані в зрозумілому форматі

### 3. Обробка помилок

- Перевіряйте повернені значення
- Логуйте втрачені події
- Моніторте статистику черги

### 4. Продуктивність

- Не виконуйте важкі операції в обробниках
- Використовуйте пріоритети розумно
- Налаштуйте розмір черги під навантаження

### 5. Безпека потоків

- `subscribe/unsubscribe` тільки з main task
- `publish` можна з будь-якого потоку
- Використовуйте `publish_from_isr` в ISR

## Типові події системи

### Системні події
- `system.startup` - запуск системи
- `system.shutdown` - вимкнення
- `system.emergency.stop` - аварійна зупинка
- `system.error` - системна помилка

### Події конфігурації
- `config.loaded` - конфігурація завантажена
- `config.changed` - зміна конфігурації
- `config.saved` - конфігурація збережена

### Події модулів
- `module.started` - модуль запущено
- `module.stopped` - модуль зупинено
- `module.error` - помилка модуля

### Події даних
- `sensor.*` - дані з сенсорів
- `actuator.*` - команди актуаторам
- `data.processed` - дані оброблено

## Діагностика

### Моніторинг статистики

```cpp
void print_eventbus_stats() {
    auto stats = EventBus::get_stats();
    ESP_LOGI(TAG, "EventBus Stats:");
    ESP_LOGI(TAG, "  Published: %d", stats.total_published);
    ESP_LOGI(TAG, "  Processed: %d", stats.total_processed);
    ESP_LOGI(TAG, "  Dropped: %d", stats.total_dropped);
    ESP_LOGI(TAG, "  Queue: %d/%d", stats.queue_size, stats.max_queue_size);
    ESP_LOGI(TAG, "  Subscriptions: %d", stats.total_subscriptions);
    ESP_LOGI(TAG, "  Avg time: %d us", stats.avg_process_time_us);
}
```

## Обмеження

- Максимальний розмір черги обмежений пам'яттю
- Обробники виконуються в main task
- Порядок обробки залежить від пріоритету
- Шаблони підписки підтримують тільки прості wildcards

## Див. також

- [Module Manager](module_manager.txt) - управління модулями
- [Config Manager](config_manager.md) - управління конфігурацією
- [Core System](core_system.md) - основна система