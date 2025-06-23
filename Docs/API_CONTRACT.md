# API Contract Documentation

Цей документ описує всі контракти міжмодульної взаємодії в системі ModESP. Він є довідником для розробників і доповнює файл `system_contract.h`.

## SharedState Keys

| Ім'я Контракту | Тип | Опис | Формат Даних | Хто Публікує | Хто Слухає |
|----------------|-----|------|--------------|--------------|------------|
| **sensor.temperature** | State | Поточна температура | `float` (°C) | SensorModule | ClimateControl, WebUI, MQTT |
| **sensor.humidity** | State | Поточна вологість | `float` (%) | SensorModule | ClimateControl, WebUI, MQTT |
| **sensor.pressure** | State | Тиск в системі | `float` (bar) | SensorModule | AlarmModule, WebUI |
| **sensor.door_open** | State | Стан дверей | `bool` | SensorModule | AlarmModule, ClimateControl |
| **actuator.compressor** | State | Стан компресора | `{"state": bool, "runtime": int}` | ActuatorModule | ClimateControl, WebUI |
| **actuator.evaporator_fan** | State | Стан вентилятора випарника | `{"state": bool, "speed": int}` | ActuatorModule | ClimateControl, WebUI |
| **actuator.defrost_heater** | State | Стан нагрівача розморозки | `{"state": bool, "power": float}` | ActuatorModule | DefrostControl, WebUI |
| **actuator.light** | State | Стан освітлення | `bool` | ActuatorModule | WebUI |
| **system.uptime** | State | Час роботи системи | `uint32_t` (секунди) | Application | WebUI, MQTT |
| **system.free_heap** | State | Вільна пам'ять | `uint32_t` (байти) | Application | MonitoringModule, WebUI |
| **system.time** | State | Поточний час | `int64_t` (Unix timestamp) | RTCModule | All modules |
| **system.time_valid** | State | Чи валідний час | `bool` | RTCModule | LoggingModule |
| **climate.setpoint** | State | Задана температура | `float` (°C) | ClimateControl | WebUI, MQTT |
| **climate.mode** | State | Режим роботи | `string` ("cooling", "heating", "idle") | ClimateControl | WebUI, ActuatorModule |
| **climate.control_active** | State | Чи активне регулювання | `bool` | ClimateControl | ActuatorModule, WebUI |
| **network.wifi_status** | State | Статус WiFi | `{"connected": bool, "ssid": string, "rssi": int}` | WiFiManager | WebUI, MQTT |
| **network.ip_address** | State | IP адреса | `string` | WiFiManager | WebUI, MQTT |
| **network.mqtt_connected** | State | Статус MQTT | `bool` | MQTTModule | WebUI |
| **api.status** | State | Статус API сервера | `{"running": bool, "port": int}` | WebUIModule | MonitoringModule |
| **api.active_connections** | State | Кількість активних з'єднань | `int` | WebUIModule | MonitoringModule |
| **api.metrics** | State | Метрики API | `{"requests": int, "errors": int, "avg_response_time": float}` | WebUIModule | MonitoringModule |

## EventBus Events

| Ім'я Контракту | Тип | Опис | Формат Payload | Хто Публікує | Хто Слухає |
|----------------|-----|------|----------------|--------------|------------|
| **sensor.reading** | Event | Нове показання датчика | `{"sensor_id": string, "value": float, "unit": string}` | SensorModule | DataLogger, WebUI |
| **sensor.error** | Event | Помилка датчика | `{"sensor_id": string, "error": string, "code": int}` | SensorModule | AlarmModule, WebUI |
| **sensor.threshold_exceeded** | Event | Перевищення порогу | `{"sensor_id": string, "value": float, "threshold": float}` | SensorModule | AlarmModule || **actuator.command** | Event | Команда для актуатора | `{"actuator_id": string, "command": string, "params": object}` | ClimateControl, WebUI | ActuatorModule |
| **actuator.state_changed** | Event | Зміна стану актуатора | `{"actuator_id": string, "old_state": any, "new_state": any}` | ActuatorModule | ClimateControl, WebUI |
| **actuator.error** | Event | Помилка актуатора | `{"actuator_id": string, "error": string, "code": int}` | ActuatorModule | AlarmModule, WebUI |
| **system.startup** | Event | Запуск системи | `{"version": string, "modules": array}` | Application | All modules |
| **system.shutdown** | Event | Вимкнення системи | `{"reason": string}` | Application | All modules |
| **system.error** | Event | Системна помилка | `{"module": string, "error": string, "severity": string}` | Any module | MonitoringModule, WebUI |
| **system.time_sync** | Event | Синхронізація часу | `{"old_time": int64_t, "new_time": int64_t, "source": string}` | RTCModule | LoggingModule |
| **climate.setpoint_changed** | Event | Зміна заданої температури | `{"old": float, "new": float, "source": string}` | ClimateControl, WebUI | ActuatorModule |
| **climate.mode_changed** | Event | Зміна режиму роботи | `{"old": string, "new": string}` | ClimateControl, WebUI | ActuatorModule |
| **climate.alarm** | Event | Аварія клімат-контролю | `{"type": string, "message": string, "severity": string}` | ClimateControl | AlarmModule, WebUI |
| **network.connected** | Event | Підключення до мережі | `{"type": string, "details": object}` | WiFiManager | WebUI, MQTT |
| **network.disconnected** | Event | Відключення від мережі | `{"type": string, "reason": string}` | WiFiManager | WebUI, MQTT |
| **network.error** | Event | Мережева помилка | `{"type": string, "error": string, "code": int}` | WiFiManager | MonitoringModule |
| **api.request** | Event | API запит | `{"method": string, "path": string, "client_id": string}` | WebUIModule | MonitoringModule |
| **api.response** | Event | API відповідь | `{"method": string, "path": string, "status": int, "time_ms": int}` | WebUIModule | MonitoringModule |
| **api.error** | Event | API помилка | `{"method": string, "path": string, "error": string, "code": int}` | WebUIModule | MonitoringModule |
| **api.client_connected** | Event | Клієнт підключився | `{"client_id": string, "type": string, "ip": string}` | WebUIModule | MonitoringModule |
| **api.client_disconnected** | Event | Клієнт відключився | `{"client_id": string, "reason": string}` | WebUIModule | MonitoringModule |

## Правила використання

1. **Іменування**: Всі імена мають бути ієрархічними з використанням крапки як роздільника
2. **Формат даних**: SharedState зберігає прості типи або JSON об'єкти, EventBus завжди передає JSON
3. **Версіонування**: При зміні формату даних необхідно оновити всі модулі, що використовують контракт
4. **Документування**: Кожен новий контракт має бути задокументований в цьому файлі

## Приклади використання

### Читання зі SharedState
```cpp
#include "system_contract.h"
using namespace ModespContract;

float temperature;
if (SharedState::get(State::SensorTemperature, temperature)) {
    ESP_LOGI(TAG, "Current temperature: %.1f°C", temperature);
}
```

### Публікація події
```cpp
#include "system_contract.h"
using namespace ModespContract;

nlohmann::json event_data = {
    {"sensor_id", "temp_chamber_1"},
    {"value", 23.5},
    {"unit", "celsius"}
};
EventBus::publish(Event::SensorReading, event_data);
```

### Підписка на подію
```cpp
#include "system_contract.h"
using namespace ModespContract;

EventBus::subscribe(Event::ClimateSetpointChanged, 
    [](const EventBus::Event& event) {
        float new_setpoint = event.data["new"];
        ESP_LOGI(TAG, "New setpoint: %.1f°C", new_setpoint);
    });
```