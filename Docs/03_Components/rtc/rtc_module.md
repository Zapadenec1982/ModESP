# RTCModule - Real-Time Clock Module

## Опис

RTCModule забезпечує базові функції роботи з часом для системи ModuChill. У першій версії використовує внутрішній RTC ESP32.

## Основні функції

1. **Timestamps для логування** - всі події в системі мають точну мітку часу
2. **Відстеження uptime** - час роботи системи з моменту старту
3. **Публікація часу в SharedState** - інші модулі можуть отримати поточний час
4. **Часові зони** - підтримка різних часових зон

## Використання

### Отримання поточного часу

```cpp
// Статичні методи доступні з будь-якого місця
time_t timestamp = RTCModule::get_timestamp();
std::string time_str = RTCModule::get_time_string();
uint32_t uptime = RTCModule::get_uptime_seconds();
```

### Читання часу з SharedState

```cpp
// Інші модулі можуть читати час
auto time_data = SharedState::get("state.time");
// {
//   "timestamp": 1704067200,
//   "time_string": "2024-01-01 00:00:00",
//   "uptime_seconds": 3600,
//   "timezone": "UTC",
//   "is_valid": true
// }
```

### Встановлення часу

```cpp
// Встановити конкретний час (наприклад, з зовнішнього джерела)
RTCModule::set_time(1704067200);  // Unix timestamp
```

## Конфігурація (rtc.json)

```json
{
    "timezone": "UTC-3",              // Часова зона
    "publish_to_shared_state": true,  // Публікувати час в SharedState
    "publish_interval_s": 60          // Інтервал публікації (секунди)
}
```

## Події

RTCModule генерує наступні події через EventBus:

- `system.rtc.initialized` - модуль ініціалізовано
- `system.rtc.time_changed` - час було змінено

## Інтеграція з іншими модулями

### ClimateControl
```cpp
// Логування з timestamp
ESP_LOGI(TAG, "[%s] Temperature: %.1f°C", 
         RTCModule::get_time_string().c_str(), temperature);
```

### Відстеження часу роботи компресора
```cpp
// Зберігаємо час вмикання
uint32_t compressor_start_time = RTCModule::get_timestamp();

// Розраховуємо час роботи
uint32_t run_time = RTCModule::get_timestamp() - compressor_start_time;
```

## Майбутні розширення

1. **Зовнішній RTC** (DS3231) - для збереження часу при відключенні живлення
2. **NTP синхронізація** - автоматична синхронізація через інтернет
3. **Планувальник подій** - виконання дій за розкладом
4. **Літній/зимовий час** - автоматичне перемикання

## Приклад логування з timestamps

```
[2024-01-01 12:00:00] System boot completed
[2024-01-01 12:00:05] Temperature sensor initialized: chamber_temp
[2024-01-01 12:00:10] Compressor ON (setpoint: 4.0°C, current: 5.2°C)
[2024-01-01 12:05:15] Compressor OFF (temperature reached: 3.8°C)
[2024-01-01 12:05:15] Compressor run time: 305 seconds
```
