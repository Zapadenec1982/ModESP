# 🧪 ModESP Core Integration Test Suite

## Огляд

Це повноцінний Integration Test Suite для всього ядра ModESP системи. На відміну від unit тестів окремих компонентів, ці тести перевіряють **інтеграцію та взаємодію всієї системи** в реальних умовах.

## Що тестується

### 🔄 System Lifecycle Tests
- **Повна ініціалізація системи** - від boot до RUNNING стану
- **Стабільність роботи** - 15+ секунд безперервної роботи  
- **Graceful shutdown** - коректне завершення всіх компонентів
- **System restart** - перезапуск системи

### 🔥 System Stress Tests  
- **High Event Throughput** - 10,000+ подій через EventBus
- **Memory Pressure** - виділення/звільнення великих об'ємів пам'яті
- **CPU Intensive Load** - багатопоточне навантаження на CPU
- **Resource Exhaustion** - тестування лімітів системи

### ⚠️ Error Scenario Tests
- **Module Error Handling** - реакція на помилки модулів
- **Configuration Errors** - обробка невалідних конфігурацій  
- **Resource Exhaustion** - поведінка при вичерпанні ресурсів
- **Hardware Failure Simulation** - симуляція збоїв hardware
- **Error Reporting System** - система звітування про помилки

### 🔄 Multicore Tests
- **Basic Multicore Operation** - робота на обох ядрах ESP32-S3
- **Inter-Core Synchronization** - синхронізація між ядрами
- **Multicore Performance** - балансування навантаження
- **Application Multicore Behavior** - реальна поведінка додатку

### 🔌 Real Hardware Tests
- **Flash Memory Operations** - NVS, конфігурації, persistence
- **GPIO Functionality** - цифрові входи/виходи, тайминги
- **Timer Accuracy** - точність високочастотних таймерів
- **System Resource Monitoring** - моніторинг пам'яті, CPU, flash
- **WiFi Hardware** - базова функціональність WiFi

## Архітектура тестів

```
test/integration_test/
├── CMakeLists.txt                 # Головна конфігурація проекту
├── sdkconfig.defaults             # ESP-IDF конфігурація
├── partitions.csv                 # Таблиця розділів flash
├── main/
│   ├── main.cpp                   # Entry point тестів
│   └── CMakeLists.txt
└── components/integration_tests/
    ├── include/
    │   └── integration_test_common.h  # Спільні утиліти
    ├── test_system_lifecycle.cpp     # Lifecycle тести
    ├── test_system_stress.cpp        # Stress тести  
    ├── test_error_scenarios.cpp      # Error тести
    ├── test_multicore.cpp            # Multicore тести
    ├── test_real_hardware.cpp        # Hardware тести
    ├── integration_test_utils.cpp    # Допоміжні функції
    └── CMakeLists.txt
```

## Як запустити тести

### Підготовка

1. **Підключіть ESP32-S3** до порту (наприклад, COM3)
2. **Перейдіть до директорії тестів:**
   ```bash
   cd test/integration_test
   ```

### Збірка та запуск

```bash
# Ініціалізація ESP-IDF
. C:\Users\User\esp\v5.3.3\esp-idf\export.ps1

# Налаштування цільової платформи
idf.py set-target esp32s3

# Збірка проекту
idf.py build

# Прошивка та моніторинг
idf.py -p COM3 flash monitor
```

### Очікувані результати

```
=== ModESP Core Integration Test Suite ===
🧪 Running System Lifecycle Tests...
✅ Full system initialization test PASSED
✅ System operation stability test PASSED  
✅ Graceful shutdown test PASSED

🔥 Running System Stress Tests...
✅ High event throughput test PASSED
✅ Memory pressure test PASSED
✅ CPU intensive load test PASSED

⚠️ Running Error Scenario Tests...
✅ Module error handling test PASSED
✅ Configuration errors test PASSED
✅ Resource exhaustion scenarios test PASSED

🔄 Running Multicore Tests...
✅ Basic multicore operation test PASSED
✅ Inter-core synchronization test PASSED
✅ Multicore performance test PASSED

🔌 Running Real Hardware Tests...
✅ Flash memory operations test PASSED
✅ GPIO functionality test PASSED
✅ Timer accuracy test PASSED

=== Integration Test Suite Complete ===
```

## Конфігурація

### Partition Table
```csv
# Name,   Type, SubType, Offset,  Size, Flags
nvs,      data, nvs,     0x9000,  0x10000,     # 64KB NVS
phy_init, data, phy,     0x19000, 0x1000,      # 4KB PHY
factory,  app,  factory, 0x20000, 0x180000,    # 1.5MB App
storage,  data, spiffs,  0x1A0000, 0x60000,    # 384KB Storage
```

### ESP-IDF Settings
- **Target:** ESP32-S3
- **Main Task Stack:** 16KB
- **FreeRTOS Frequency:** 100Hz
- **WiFi:** Enabled
- **Debug:** Full debugging enabled

## Метрики та пороги

### Memory Thresholds
- **Minimum Free Heap:** 100KB
- **Critical Free Heap:** 50KB  
- **Max Memory Leak:** 15KB per test

### Performance Thresholds
- **Max Cycle Time:** 10ms
- **Max CPU Usage:** 80%
- **Min Stack Free:** 1KB

### Stability Requirements
- **Health Check Success Rate:** 95%
- **System Uptime:** 15+ seconds continuous
- **Memory Stability:** <50KB variance

## Переваги Integration Testing

### ✅ Реальні умови
- Тестування на справжньому ESP32-S3 hardware
- Реальні тайминги FreeRTOS та ESP-IDF
- Справжнє навантаження на пам'ять та CPU

### ✅ Виявлення проблем інтеграції  
- Race conditions між компонентами
- Memory leaks в реальних сценаріях
- Performance bottlenecks під навантаженням
- Timing issues в multicore середовищі

### ✅ Валідація системи в цілому
- Перевірка всього lifecycle від boot до shutdown
- Тестування error recovery механізмів
- Валідація multicore координації
- Перевірка hardware взаємодії

## Troubleshooting

### Якщо тести падають:

1. **Memory Issues:**
   - Перевірте heap usage в логах
   - Збільште partition sizes якщо потрібно
   - Перевірте memory leaks в компонентах

2. **Timing Issues:**
   - Збільште timeouts в тестах
   - Перевірте watchdog settings
   - Оптимізуйте priority задач

3. **Hardware Issues:**
   - Перевірте підключення ESP32-S3
   - Переконайтеся що порт правильний
   - Перевірте flash memory стан

## Результат

Успішне проходження всіх Integration Tests **гарантує**, що:

- 🎯 **Ядро ModESP готове до продакшн використання**
- 🔒 **Система стабільна під навантаженням**  
- 🚀 **Performance відповідає вимогам**
- 🛡️ **Error handling працює коректно**
- ⚡ **Multicore координація функціонує**
- 🔧 **Hardware інтеграція успішна**

**Це справжня валідація всієї системи як єдиного цілого!** 🎉 