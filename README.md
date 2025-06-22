# ModuChill - Модульна прошивка для промислових холодильних установок

## Огляд

ModuChill - це модульна прошивка для ESP32, призначена для управління:
- Промисловими холодильниками та морозильними камерами
- Камерами дозрівання
- Торговим холодильним обладнанням
- Системами кондиціонування

## Ключові особливості

### 🏗️ Модульна архітектура
- Повністю модульна система з можливістю додавання/видалення компонентів
- Автоматична реєстрація драйверів
- Конфігурація через JSON файли
- Мінімальне споживання пам'яті (< 5KB RAM для ядра)

### 🔧 Підтримувані датчики
- **DS18B20** - цифрові датчики температури OneWire
- **NTC термістори** - аналогові датчики температури
- **Датчики тиску 4-20мА** - промислові датчики
- **GPIO входи** - дискретні сигнали

### ⚡ Підтримувані актуатори
- **Реле** - компресори, нагрівачі, клапани
- **PWM керування** - вентилятори, клапани з модуляцією
- **GPIO виходи** - індикація, сигналізація

### 🌐 Мережеві можливості
- WiFi підключення
- Web інтерфейс
- MQTT інтеграція
- OTA оновлення

### 🎛️ Користувацький інтерфейс
- Веб-інтерфейс з адаптивним дизайном
- Підтримка дисплеїв
- Схемо-орієнтований UI

## Підтримувані платформи

| Платформа | Статус | Особливості |
|-----------|---------|-------------|
| ESP32 | ✅ Повна підтримка | Стандартна конфігурація |
| ESP32-S3 | ✅ Рекомендована | Оптимізована для цієї платформи |
| ESP32-C3 | ⚠️ Обмежена | Без деяких функцій |
| ESP32-S2 | ⚠️ Обмежена | Без деяких функцій |

## Швидкий старт

### Вимоги
- ESP-IDF v5.0+
- ESP32-S3 (рекомендовано) або ESP32
- VSCode з ESP-IDF розширенням

### Встановлення

1. **Клонування проекту**:
```bash
git clone <repository-url>
cd ModESP_dev
```

2. **Налаштування ESP-IDF**:
```bash
idf.py set-target esp32s3
idf.py menuconfig
```

3. **Конфігурація**:
   - Налаштуйте GPIO у файлі `components/ESPhal/board_config.h`
   - Відредагуйте конфігурації датчиків у `components/core/configs/sensors.json`
   - Налаштуйте актуатори у `components/core/configs/actuators.json`

4. **Збірка та прошивка**:
```bash
idf.py build
idf.py flash monitor
```

### Базова конфігурація

#### Налаштування датчика температури (sensors.json):
```json
{
  "sensors": [{
    "role": "chamber_temp",
    "type": "DS18B20",
    "config": {
      "hal_id": "ONEWIRE_CHAMBER",
      "address": "28ff640264013c28"
    }
  }]
}
```

#### Налаштування компресора (actuators.json):
```json
{
  "actuators": [{
    "role": "compressor",
    "type": "RELAY",
    "command_key": "command.actuator.compressor",
    "config": {
      "hal_id": "RELAY_COMPRESSOR",
      "min_off_time_s": 180
    }
  }]
}
```

## Архітектура системи

```
Application Layer (Бізнес-логіка)
    ├── ClimateControl - Керування температурою
    ├── DefrostControl - Управління розморожуванням
    └── AlarmSystem - Система сигналізації
                    ↕
               SharedState - Централізований обмін даними
                    ↕
Hardware Abstraction Layer (HAL)
    ├── SensorModule - Збір даних з датчиків
    ├── ActuatorModule - Керування виконавчими механізмами
    ├── UI System - Інтерфейси користувача
    └── Network - Мережеві комунікації
                    ↕
Driver Layer (Драйвери)
    ├── sensor_drivers/ - DS18B20, NTC, GPIO, Pressure
    ├── actuator_drivers/ - Relay, PWM, GPIO Output
    └── ESPhal/ - Низькорівневі HAL інтерфейси
```

## Структура проекту

```
ModESP_dev/
├── main/                     # Точка входу ESP-IDF
├── components/               # Модульні компоненти
│   ├── core/                # Основне ядро системи
│   ├── ESPhal/              # Hardware Abstraction Layer
│   ├── sensor_drivers/      # Драйвери датчиків
│   ├── actuator_drivers/    # Драйвери актуаторів
│   ├── ui/                  # Користувацький інтерфейс
│   └── wifi_manager/        # Мережеві функції
├── Docs/                    # Документація
└── configs/                 # Конфігураційні файли
```

## Конфігурація через menuconfig

```
ModuChill Configuration
├── Sensor Drivers
│   ├── [*] Enable DS18B20 Temperature Sensor
│   ├── [*] Enable NTC Thermistor Support
│   ├── [*] Enable 4-20mA Pressure Sensors
│   └── [*] Enable GPIO Input Drivers
├── Actuator Drivers
│   ├── [*] Enable Relay Control
│   ├── [*] Enable PWM Control
│   └── [*] Enable GPIO Output Control
├── Business Logic Modules
│   ├── [*] Enable Climate Control
│   ├── [*] Enable Defrost Control
│   └── [*] Enable Alarm System
└── User Interface
    ├── [*] Enable Web Interface
    ├── [*] Enable Display Support
    └── [*] Enable MQTT Integration
```

## Додавання нових компонентів

### Новий драйвер датчика

1. Створіть компонент у `components/sensor_drivers/my_sensor/`
2. Реалізуйте інтерфейс `ISensorDriver`
3. Додайте автореєстрацію драйвера
4. Використайте в `sensors.json`

Детальніше: [Docs/SensorModule.txt](Docs/SensorModule.txt)

### Новий драйвер актуатора

1. Створіть компонент у `components/actuator_drivers/my_actuator/`
2. Реалізуйте інтерфейс `IActuatorDriver`
3. Додайте автореєстрацію драйвера
4. Використайте в `actuators.json`

Детальніше: [Docs/ActuatorModule.txt](Docs/ActuatorModule.txt)

## Веб-інтерфейс

Після прошивки пристрій створює WiFi точку доступу або підключається до налаштованої мережі.
Веб-інтерфейс доступний за адресою: `http://192.168.4.1` (або IP пристрою в мережі)

Функції веб-інтерфейсу:
- Моніторинг датчиків у реальному часі
- Керування актуаторами
- Налаштування параметрів системи
- Перегляд логів та діагностики
- OTA оновлення прошивки

## Документація

| Документ | Опис |
|----------|------|
| [SYSTEM_ARCHITECTURE.md](Docs/SYSTEM_ARCHITECTURE.md) | Повна архітектура системи |
| [GETTING_STARTED.md](Docs/GETTING_STARTED.md) | Швидкий старт для розробників |
| [TODO.md](Docs/TODO.md) | Структурований список завдань |
| [ACTION_PLAN.md](Docs/ACTION_PLAN.md) | Конкретні наступні кроки |
| [QUICK_START_CHECKLIST.md](Docs/QUICK_START_CHECKLIST.md) | 15-хвилинний setup checklist |
| [DEVELOPMENT_GUIDELINES.md](Docs/DEVELOPMENT_GUIDELINES.md) | Стандарти коду та best practices |
| [GIT_WORKFLOW.md](Docs/GIT_WORKFLOW.md) | Git workflow та code review |
| [Core.txt](Docs/Core.txt) | Документація Core модуля |
| [SensorModule.txt](Docs/SensorModule.txt) | Робота з датчиками |
| [ActuatorModule.txt](Docs/ActuatorModule.txt) | Керування актуаторами |

**Повний індекс документації**: [Docs/README.md](Docs/README.md)

## Технічні характеристики

- **Споживання пам'яті**: < 5KB RAM для ядра
- **Частота головного циклу**: 100Hz
- **Кількість підтримуваних датчиків**: До 64 одночасно
- **Кількість підтримуваних актуаторів**: До 32 одночасно
- **Час відгуку на команди**: < 10ms
- **Період опитування датчиків**: Конфігурований (за замовчуванням 1Hz)

## Розробка та внесок

Система розроблена з акцентом на:
- **Модульність** - легке додавання нових функцій
- **Надійність** - промислова якість коду
- **Продуктивність** - оптимізація для ESP32
- **Розширюваність** - готовність до майбутніх вимог

## Ліцензія

[LICENSE](LICENSE)

## Контакти

Для питань щодо розробки та використання звертайтесь до команди розробників.

---

**ModuChill** - Professional Industrial Refrigeration Control System