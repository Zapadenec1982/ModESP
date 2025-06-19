# Оптимізація використання пам'яті драйверами

## Як працює система драйверів

### 1. Реєстрація (при старті)
При запуску системи всі включені драйвери реєструються в `SensorDriverRegistry`:
```cpp
// Реєструється тільки фабрична функція!
// Сам об'єкт драйвера НЕ створюється
registry.register_driver("DS18B20", factory_function);
```

### 2. Створення (при ініціалізації)
Драйвери створюються ТІЛЬКИ якщо вони вказані в `sensors.json`:
```cpp
// Створюється екземпляр тільки для type="DS18B20"
if (config.type == "DS18B20") {
    auto driver = registry.create_driver("DS18B20");
}
```

### 3. Виконання (під час роботи)
Код виконується ТІЛЬКИ для створених драйверів:
```cpp
// Якщо використовується NTC, код DS18B20 НЕ викликається
for (auto& sensor : active_sensors) {
    sensor.driver->read(); // Тільки активні драйвери
}
```

## Оптимізація розміру прошивки

### Проблема
Навіть якщо драйвер не використовується, його код все одно компілюється і займає Flash пам'ять:
- DS18B20: ~15 KB
- DS18B20_Async: ~18 KB  
- NTC: ~10 KB
- Всього: ~43 KB (навіть якщо використовується тільки один!)

### Рішення - Kconfig
Тепер можна вимкнути непотрібні драйвери через menuconfig:

```bash
idf.py menuconfig
# Component config → Sensor Drivers Configuration
```

Або в sdkconfig:
```
# Вимкнути непотрібні драйвери
CONFIG_SENSOR_DRIVER_DS18B20_ENABLED=n
CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED=y
CONFIG_SENSOR_DRIVER_NTC_ENABLED=y
```

### Результат
- Компілюються ТІЛЬКИ включені драйвери
- Економія Flash: до 30-40 KB
- Швидша компіляція
- Менший розмір OTA оновлень

## Приклади конфігурацій

### Тільки NTC термістори
```
CONFIG_SENSOR_DRIVER_NTC_ENABLED=y
CONFIG_SENSOR_DRIVER_DS18B20_ENABLED=n
CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED=n
```
Економія: ~33 KB

### Тільки DS18B20 (асинхронні)
```
CONFIG_SENSOR_DRIVER_NTC_ENABLED=n
CONFIG_SENSOR_DRIVER_DS18B20_ENABLED=n
CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED=y
```
Економія: ~25 KB

### Повний набір (за замовчуванням)
```
CONFIG_SENSOR_DRIVER_NTC_ENABLED=y
CONFIG_SENSOR_DRIVER_DS18B20_ENABLED=y
CONFIG_SENSOR_DRIVER_DS18B20_ASYNC_ENABLED=y
```
Всі драйвери доступні

## Висновок

1. **Runtime**: Код невикористаних драйверів НЕ виконується
2. **Compile time**: За замовчуванням всі драйвери компілюються
3. **Оптимізація**: Використовуйте Kconfig для вимкнення непотрібних драйверів
