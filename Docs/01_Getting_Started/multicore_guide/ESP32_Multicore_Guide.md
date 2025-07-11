# ESP32 Multicore Guide для ModESP

## 🚀 Основи багатоядерності ESP32

### Архітектура ESP32
- **Core 0 (PRO_CPU)**: Протокольний процесор - WiFi, Bluetooth, системні задачі
- **Core 1 (APP_CPU)**: Прикладний процесор - користувацькі задачі

### Розподіл задач в ESP-IDF

#### ❌ НЕ автоматично:
```c
// Ця задача буде на тому ж ядрі, де викликана
xTaskCreate(task_func, "task", 4096, NULL, 5, &handle);
```

#### ✅ Явне призначення:
```c
// Явно вказуємо ядро
xTaskCreatePinnedToCore(
    task_func,      // Функція
    "task_name",    // Ім'я
    4096,          // Стек
    NULL,          // Параметри
    5,             // Пріоритет
    &handle,       // Handle
    0              // Core ID (0 або 1)
);
```

## 📋 Рекомендований розподіл для ModESP
### Core 0 (WiFi/Network Core)
- WiFi management
- MQTT client
- Web server / HTTP API
- OTA updates
- Network protocols

### Core 1 (Application Core)
- Sensor reading
- Temperature control algorithms
- Data processing
- Local UI (LCD, buttons)
- Modbus communication
- Data logging

## 🔧 Практичний приклад для ModESP

```cpp
// application.cpp
void launch_application() {
    // Критичні системні задачі на Core 0
    xTaskCreatePinnedToCore(
        network_manager_task,
        "net_mgr",
        4096,
        NULL,
        8,      // Високий пріоритет
        &net_task_handle,
        0       // Core 0
    );
    
    // Сенсори та контроль на Core 1    xTaskCreatePinnedToCore(
        sensor_controller_task,
        "sensor_ctrl",
        6144,
        NULL,
        7,      // Високий пріоритет для реального часу
        &sensor_task_handle,
        1       // Core 1
    );
    
    // Модулі UI на Core 1 (менший пріоритет)
    xTaskCreatePinnedToCore(
        display_update_task,
        "display",
        4096,
        NULL,
        3,
        &display_task_handle,
        1       // Core 1
    );
}
```

## ⚠️ Важливі моменти

### 1. Синхронізація між ядрами
```cpp
// Безпечний обмін даними
SemaphoreHandle_t data_mutex = xSemaphoreCreateMutex();
QueueHandle_t inter_core_queue = xQueueCreate(10, sizeof(sensor_data_t));

// Core 0: Відправка даних
xSemaphoreTake(data_mutex, portMAX_DELAY);
sensor_data_t data = read_sensors();
xSemaphoreGive(data_mutex);xQueueSend(inter_core_queue, &data, pdMS_TO_TICKS(10));
```

### 2. Cache Coherency
```cpp
// При роботі зі спільними даними
volatile uint32_t shared_counter;  // volatile для видимості між ядрами

// Або використовуйте атомарні операції
#include "freertos/atomic.h"
Atomic_Increment_u32(&shared_counter);
```

### 3. Прив'язка переривань
```cpp
// Прив'язка ISR до конкретного ядра
esp_err_t err = esp_intr_alloc(
    ETS_GPIO_INTR_SOURCE,
    ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_CPU1,  // ISR на Core 1
    gpio_isr_handler,
    NULL,
    &intr_handle
);
```

## 📊 Моніторинг навантаження

```cpp
// Перевірка використання ядер
void print_core_usage() {
    TaskStatus_t *task_status_array;
    volatile UBaseType_t task_count;
    
    task_count = uxTaskGetNumberOfTasks();    task_status_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));
    
    uxTaskGetSystemState(task_status_array, task_count, NULL);
    
    for (int i = 0; i < task_count; i++) {
        ESP_LOGI("CORE", "Task: %s, Core: %d, Stack HWM: %d",
                 task_status_array[i].pcTaskName,
                 task_status_array[i].xCoreID,
                 task_status_array[i].usStackHighWaterMark);
    }
    
    vPortFree(task_status_array);
}
```

## 🎯 Оптимізації для ModESP

### 1. Розділення за частотою оновлення
- **Core 0**: Повільні задачі (мережа, логування) - 100-1000ms
- **Core 1**: Швидкі задачі (сенсори, PID) - 10-50ms

### 2. Пріоритети задач
```cpp
// Core 0 (мережеві задачі)
#define WIFI_TASK_PRIORITY      8
#define MQTT_TASK_PRIORITY      7
#define HTTP_TASK_PRIORITY      6
#define LOG_TASK_PRIORITY       4

// Core 1 (реальний час)
#define SENSOR_TASK_PRIORITY    9
#define CONTROL_TASK_PRIORITY   8
#define DISPLAY_TASK_PRIORITY   5
#define UI_TASK_PRIORITY        4
```
### 3. Приклад для холодильної системи

```cpp
// refrigeration_controller.cpp
class RefrigerationController {
private:
    // Handles для задач
    TaskHandle_t sensor_task_handle;
    TaskHandle_t control_task_handle;
    TaskHandle_t network_task_handle;
    TaskHandle_t ui_task_handle;
    
    // Черги для міжядерної комунікації
    QueueHandle_t sensor_to_control_queue;
    QueueHandle_t control_to_network_queue;
    
public:
    void init() {
        // Створюємо черги
        sensor_to_control_queue = xQueueCreate(20, sizeof(SensorData));
        control_to_network_queue = xQueueCreate(10, sizeof(ControlStatus));
        
        // Core 1: Критичні задачі реального часу
        xTaskCreatePinnedToCore(
            sensor_reading_task,
            "sensors",
            4096,
            this,
            9,  // Найвищий пріоритет
            &sensor_task_handle,
            1   // Core 1
        );
        
        xTaskCreatePinnedToCore(
            temperature_control_task,
            "temp_ctrl",            6144,
            this,
            8,
            &control_task_handle,
            1   // Core 1
        );
        
        // Core 0: Мережеві задачі
        xTaskCreatePinnedToCore(
            network_communication_task,
            "network",
            8192,  // Більший стек для SSL/TLS
            this,
            6,
            &network_task_handle,
            0   // Core 0
        );
        
        // Core 1: UI (низький пріоритет)
        xTaskCreatePinnedToCore(
            user_interface_task,
            "ui",
            4096,
            this,
            3,
            &ui_task_handle,
            1   // Core 1
        );
    }
    
    static void sensor_reading_task(void* pvParameters) {
        RefrigerationController* self = (RefrigerationController*)pvParameters;
        TickType_t xLastWakeTime = xTaskGetTickCount();
        
        while (1) {
            // Читання сенсорів кожні 50ms
            SensorData data = {                .temperature = read_temperature(),
                .humidity = read_humidity(),
                .pressure = read_pressure(),
                .timestamp = esp_timer_get_time()
            };
            
            // Відправка в чергу контролера
            xQueueSend(self->sensor_to_control_queue, &data, 0);
            
            // Точна затримка 50ms
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(50));
        }
    }
};
```

## 🚨 Типові помилки

1. **Всі задачі на одному ядрі** - використовуйте `xTaskCreatePinnedToCore()`
2. **Блокування WiFi задач** - Core 0 має бути вільним для WiFi/BT
3. **Неправильні пріоритети** - критичні задачі повинні мати вищий пріоритет
4. **Відсутність синхронізації** - використовуйте mutex/semaphore для спільних даних

## 📈 Результат оптимізації

При правильному розподілі задач:
- ⚡ Зменшення затримок на 40-60%
- 🎯 Покращення стабільності контролю температури
- 📡 Стабільніша робота WiFi/MQTT
- 🔋 Ефективніше використання ресурсів

---

**Важливо**: Завжди тестуйте багатоядерну конфігурацію під навантаженням!
