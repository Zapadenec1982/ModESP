# Custom Board Configuration

## Зміни в проекті ModESP

### 1. Нова конфігурація плати

Створено нову конфігурацію плати `custom_board.h` з наступними GPIO:

#### Реле (Outputs):
- **Relay 1**: GPIO1
- **Relay 2**: GPIO2  
- **Relay 3**: GPIO3
- **Relay 4**: GPIO4

#### Кнопки (Inputs):
- **Button 1**: GPIO9
- **Button 2**: GPIO10
- **Button 3**: GPIO12
- **Button 4**: GPIO13
- **Button 5**: GPIO11

#### Дисплей OLED (I2C):
- **SCL**: GPIO15
- **SDA**: GPIO16
- **I2C Speed**: 400kHz

#### Датчики DS18B20 (OneWire):
- **Sensor 1**: GPIO8 (ONEWIRE_BUS_1)
- **Sensor 2**: GPIO7 (ONEWIRE_BUS_2)

### 2. Видалення старого драйвера DS18B20

✅ Видалено блокуючий драйвер DS18B20:
- Видалено папку `components/sensor_drivers/ds18b20/`
- Оновлено Kconfig для видалення опції `CONFIG_SENSOR_DRIVER_DS18B20_ENABLED`
- Оновлено CMakeLists.txt та sensor_driver_init.cpp

### 3. Використання асинхронного драйвера

Тепер доступний тільки асинхронний драйвер `DS18B20_Async`, який:
- Не блокує систему
- Використовує state machine
- Підтримує кешування даних

### 4. Як увімкнути нову конфігурацію

1. Запустіть menuconfig:
```bash
idf.py menuconfig
```

2. Перейдіть до:
```
Component config → ESPhal Configuration → Board Type
```

3. Виберіть:
```
[x] Custom Board
```

4. Збережіть та вийдіть

### 5. Приклади конфігурацій

Створені приклади конфігурацій для нової плати:

- `sensors_custom_board.json` - конфігурація датчиків DS18B20
- `actuators_custom_board.json` - конфігурація реле
- `inputs_custom_board.json` - конфігурація кнопок

### 6. Використання в коді

```cpp
// Приклад використання реле
SharedState::set("actuators.relay_1", true);  // Увімкнути реле 1
SharedState::set("actuators.relay_2", false); // Вимкнути реле 2

// Читання кнопок
auto button1 = SharedState::get<bool>("state.input.button_1");
if (button1.has_value() && button1.value()) {
    // Кнопка 1 натиснута
}

// Читання температури
auto temp1 = SharedState::get<float>("state.sensor.temperature_1");
if (temp1.has_value()) {
    ESP_LOGI(TAG, "Temperature 1: %.2f°C", temp1.value());
}
```

### 7. Налаштування модулів

Для оптимальної роботи з асинхронними драйверами:

```json
// В system.json
{
  "modules": {
    "SensorModule": {
      "enabled": true,
      "update_interval_ms": 100
    }
  }
}
```

## Результат

- ✅ Нова конфігурація плати створена
- ✅ Старий блокуючий драйвер DS18B20 видалено
- ✅ Система готова до використання з новою платою
- ✅ Всі драйвери оптимізовані для неблокуючої роботи
