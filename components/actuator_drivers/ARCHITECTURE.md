# ActuatorModule - Модульна архітектура виконавчих механізмів

## Огляд

ActuatorModule створює симетрію з SensorModule, забезпечуючи єдину точку керування всіма виконавчими механізмами в системі. Використовує ту саму модульну архітектуру з реєстром драйверів.

## Архітектура

```
SharedState (команди) → ActuatorModule → Драйвери → Апаратура
                            ↓
                    ActuatorDriverRegistry
                            ↓
                    ┌──────────────┐
                    │ RelayDriver  │
                    │ PwmDriver    │
                    │ StepperDriver│
                    └──────────────┘
```

## Потік роботи

1. **Команда від бізнес-логіки**:
   ```json
   SharedState::set("command.actuator.compressor", true)
   ```

2. **ActuatorModule отримує команду**:
   - Слухає зміни в SharedState через підписку
   - Знаходить відповідний драйвер за роллю
   - Передає команду драйверу

3. **Драйвер виконує команду**:
   - Перевіряє обмеження (min on/off time)
   - Керує апаратурою через HAL
   - Повертає статус виконання

4. **Публікація статусу**:
   ```json
   SharedState::set("state.actuator.compressor", {
     "is_active": true,
     "current_value": 1.0,
     "state": "RUNNING",
     "is_healthy": true
   })
   ```

## Типи драйверів

### RelayDriver
- Бінарне керування (вкл/викл)
- Захист мінімального часу вкл/викл
- Затримка для пускового струму
- Підтримка active low/high

### PwmDriver  
- Керування ШІМ (0-100%)
- Плавний старт/стоп
- Гамма-корекція для LED
- Обмеження діапазону

### Майбутні драйвери
- **StepperDriver**: Крокові двигуни з позиціонуванням
- **ServoDriver**: Сервоприводи з контролем кута
- **HBridgeDriver**: Двонаправлені DC двигуни

## Конфігурація

### actuators.json
```json
{
  "actuators": [
    {
      "role": "compressor",
      "type": "RELAY",
      "command_key": "command.actuator.compressor", 
      "status_key": "state.actuator.compressor",
      "config": {
        "hal_id": "RELAY_COMPRESSOR",
        "min_off_time_s": 180,
        "min_on_time_s": 60
      }
    },
    {
      "role": "fan_speed",
      "type": "PWM",
      "command_key": "command.actuator.fan_speed",
      "status_key": "state.actuator.fan_speed", 
      "config": {
        "gpio_num": 25,
        "frequency": 25000,
        "min_duty_percent": 20,
        "max_duty_percent": 100,
        "ramp_time_ms": 2000
      }
    }
  ]
}
```

## Додавання нового драйвера

### 1. Створити компонент
```
components/actuator_drivers/my_driver/
├── include/
│   └── my_driver.h
├── src/
│   └── my_driver.cpp
└── CMakeLists.txt
```

### 2. Реалізувати інтерфейс
```cpp
class MyDriver : public IActuatorDriver {
    // Реалізувати всі методи інтерфейсу
    esp_err_t execute_command(const nlohmann::json& command) override;
    // ...
};

// Автореєстрація
static ActuatorDriverRegistrar<MyDriver> registrar("MY_DRIVER");
```

### 3. Використати в конфігурації
```json
{
  "role": "my_actuator",
  "type": "MY_DRIVER",
  "config": { /* параметри */ }
}
```

## Безпека

### Аварійна зупинка
```cpp
actuator_module->emergency_stop_all();
```
Всі актуатори переходять в безпечний стан.

### Захисні таймери
- Мінімальний час вкл/викл для реле
- Обмеження швидкості зміни для PWM
- Блокування конфліктних команд

## Переваги архітектури

1. **Симетрія з SensorModule**
   - Однакові принципи роботи
   - Легке розуміння коду

2. **Повна модульність**
   - Драйвери як окремі компоненти
   - Легке додавання нових типів

3. **Ізоляція від бізнес-логіки**
   - ClimateControl не знає про типи актуаторів
   - Зміна актуатора = зміна конфігурації

4. **Розширюваність**
   - Додати новий тип = створити драйвер
   - Без змін в ActuatorModule

## Приклад використання

### Команда від ClimateControl
```cpp
// ClimateControl просто встановлює команду
SharedState::set("command.actuator.compressor", true);
```

### ActuatorModule обробляє
```cpp
// Автоматично:
// 1. Знаходить RelayDriver для компресора
// 2. Перевіряє min_off_time
// 3. Вмикає реле через HAL
// 4. Публікує статус
```

Ця архітектура забезпечує максимальну гнучкість та модульність системи керування!