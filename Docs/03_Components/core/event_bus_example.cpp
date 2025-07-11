# EventBus Example - Приклад використання

```cpp
#include "event_bus.h"
#include "esp_log.h"
#include "nlohmann/json.hpp"

static const char* TAG = "EventBusExample";

// Обробники подій
void handle_temperature(const EventBus::Event& event) {
    float temp = event.data["value"];
    const char* unit = event.data["unit"].get<std::string>().c_str();
    ESP_LOGI(TAG, "Temperature: %.1f %s", temp, unit);
}

void handle_all_sensors(const EventBus::Event& event) {
    ESP_LOGI(TAG, "Sensor event: %s", event.type.c_str());
    ESP_LOGI(TAG, "Data: %s", event.data.dump().c_str());
}

void handle_critical(const EventBus::Event& event) {
    if (event.priority == EventBus::Priority::CRITICAL) {
        ESP_LOGE(TAG, "CRITICAL EVENT: %s", event.type.c_str());
        // Вжити термінові заходи
    }
}

// Ініціалізація
void init_event_system() {
    // Ініціалізація EventBus
    EventBus::init(64);
    
    // Підписка на конкретні події
    EventBus::subscribe("sensor.temperature", handle_temperature);
    
    // Підписка на групу подій
    EventBus::subscribe("sensor.*", handle_all_sensors);
    
    // Підписка на всі події для моніторингу критичних
    EventBus::subscribe("", handle_critical);
}

// Публікація подій
void publish_sensor_data() {
    // Температура
    EventBus::publish("sensor.temperature", {
        {"value", 23.5},
        {"unit", "celsius"},
        {"sensor_id", "temp_01"}
    });
    
    // Вологість
    EventBus::publish("sensor.humidity", {
        {"value", 65.0},
        {"unit", "percent"},
        {"sensor_id", "hum_01"}
    }, EventBus::Priority::NORMAL);
}

// Критична подія
void emergency_stop(const char* reason) {
    EventBus::publish("system.emergency_stop", {
        {"reason", reason},
        {"timestamp", esp_timer_get_time()},
        {"action", "stop_all_actuators"}
    }, EventBus::Priority::CRITICAL);
}

// Головний цикл
void app_main() {
    init_event_system();
    
    // Симуляція роботи
    int counter = 0;
    
    while (true) {
        // Обробка подій (до 5мс)
        size_t processed = EventBus::process(5);
        
        // Періодична публікація даних
        if (++counter % 100 == 0) {  // Кожну секунду
            publish_sensor_data();
        }
        
        // Перевірка критичних умов
        if (counter % 500 == 0) {  // Кожні 5 секунд
            auto stats = EventBus::get_stats();
            ESP_LOGI(TAG, "Events: pub=%d, proc=%d, drop=%d", 
                     stats.total_published, 
                     stats.total_processed, 
                     stats.total_dropped);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```