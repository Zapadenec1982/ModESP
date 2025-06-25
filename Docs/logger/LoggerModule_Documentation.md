# LoggerModule - Документація

## Огляд

LoggerModule - це модуль логування для системи ModuChill, який забезпечує збереження та управління логами з використанням файлової системи LittleFS. Модуль оптимізований для промислового холодильного обладнання з підтримкою HACCP.

## Основні можливості

- **LittleFS файлова система** - надійне зберігання з wear leveling
- **Розмір сховища** - 1MB для логів та інших файлів
- **Автоматична ротація** - при досягненні 64KB на файл
- **Асинхронний запис** - мінімальний вплив на продуктивність
- **HACCP сумісність** - спеціальні функції для харчової безпеки
- **Фільтрація та експорт** - CSV/JSON формати

## Архітектура

```
┌─────────────────┐    ┌──────────────────┐    ┌─────────────────┐
│   RAM Buffer    │    │   LittleFS       │    │   EventBus      │
│   256 entries   │───▶│   current.log    │◄───│   Integration   │
│   ~12KB         │    │   archive_*.log  │    │   Auto-logging  │
└─────────────────┘    │   critical.log   │    └─────────────────┘
                       │   Total: 1MB     │
                       └──────────────────┘
```

## Конфігурація

### Таблиця розділів (partitions.csv) - 4MB Flash
```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,     # 20KB
otadata,  data, ota,     0xE000,   0x2000,     # 8KB
phy_init, data, phy,     0x10000,  0x1000,     # 4KB
factory,  app,  factory, 0x11000,  0x170000,   # 1.44MB
ota_0,    app,  ota_0,   0x181000, 0x170000,   # 1.44MB
storage,  data, littlefs,0x2F1000, 0x100000,   # 1MB
```

### Параметри LoggerModule
```cpp
struct LogConfig {
    size_t maxFileSize = 64 * 1024;      // 64KB на файл
    size_t maxTotalSize = 256 * 1024;    // 256KB для логів
    uint8_t maxArchiveFiles = 3;         // 3 архівні файли
    uint32_t flushInterval = 5000;       // Запис кожні 5 сек
};
```

## API

### Базове логування
```cpp
// Макроси для зручного використання
MLOG_DEBUG("Debug message: %s", details);
MLOG_INFO("System started");
MLOG_WARNING("Temperature approaching limit: %.1f", temp);
MLOG_ERROR("Sensor error: %d", errorCode);
MLOG_CRITICAL("System failure!");

// Прямі виклики
Logger::log(LogLevel::INFO, "MODULE", "Message");
```

### Структуровані події
```cpp
// Логування подій холодильного обладнання
Logger::logEvent(EventCode::TEMP_ALARM_HIGH, temperature);
Logger::logSensorData(sensorId, value);
Logger::logCompressorCycle(true, runtime);
Logger::logDefrostCycle(DefrostPhase::ACTIVE, duration);
Logger::logHACCPEvent(EventCode::HACCP_TEMP_VIOLATION, temp);
```

### Читання та експорт
```cpp
// Фільтрація логів
LogFilter filter;
filter.startTime = timestamp - 3600;  // Остання година
filter.level = LogLevel::WARNING;
filter.module = "SENSOR";
auto logs = Logger::readLogs(filter);

// Експорт
std::string csvOutput;
Logger::exportLogs("csv", csvOutput);
```

## Формат запису

```cpp
struct LogEntry {
    uint32_t timestamp;      // Unix timestamp
    uint16_t milliseconds;   // Мілісекунди
    uint8_t level;          // Рівень логування
    uint8_t moduleId;       // ID модуля
    uint16_t eventCode;     // Код події
    int32_t value;          // Числове значення
    char message[32];       // Коротке повідомлення
} __attribute__((packed)); // 48 байтів
```

## Події для холодильного обладнання

```cpp
enum EventCode {
    // Температурні події
    TEMP_ALARM_HIGH = 0x0100,
    TEMP_ALARM_LOW,
    TEMP_NORMAL,
    SENSOR_FAIL,
    
    // Розморозка
    DEFROST_START = 0x0200,
    DEFROST_END,
    DEFROST_TIMEOUT,
    DEFROST_ABORT,
    
    // Компресор
    COMPRESSOR_ON = 0x0300,
    COMPRESSOR_OFF,
    COMPRESSOR_PROTECT,
    COMPRESSOR_FAIL,
    
    // HACCP
    HACCP_TEMP_VIOLATION = 0x0400,
    HACCP_DOOR_ALARM,
    HACCP_POWER_FAIL
};
```

## Використання пам'яті

- **RAM**: ~6KB (буфер + робочі дані)
- **Flash**: 1MB (LittleFS партиція)
- **CPU**: <1% (асинхронний запис)

## Приклади використання

### Ініціалізація
```cpp
// LoggerModule автоматично ініціалізується ModuleManager
// Конфігурація через config.json
```

### Логування температурних даних
```cpp
void onTemperatureRead(float temp, uint8_t sensorId) {
    // Звичайне логування
    MLOG_INFO("Temp sensor %d: %.1f°C", sensorId, temp);
    
    // Структуроване логування для аналізу
    Logger::logSensorData(sensorId, temp);
    
    // Перевірка меж для HACCP
    if (temp > MAX_TEMP) {
        Logger::logHACCPEvent(EventCode::HACCP_TEMP_VIOLATION, temp);
        MLOG_CRITICAL("HACCP violation! Temp: %.1f°C", temp);
    }
}
```

### Логування циклів розморозки
```cpp
void DefrostController::startDefrost() {
    MLOG_INFO("Starting defrost cycle");
    Logger::logDefrostCycle(DefrostPhase::START, 0);
    
    // ... код розморозки ...
}
```

### Веб-інтерфейс інтеграція
```cpp
// REST API endpoints
server.on("/api/logs/recent", HTTP_GET, [](AsyncWebServerRequest *request){
    LogFilter filter;
    filter.count = 50;
    auto logs = Logger::readLogs(filter);
    // ... форматування відповіді ...
});

server.on("/api/logs/export/csv", HTTP_GET, [](AsyncWebServerRequest *request){
    std::string csv;
    Logger::exportLogs("csv", csv);
    request->send(200, "text/csv", csv.c_str());
});
```

## Статистика та моніторинг

```cpp
// Отримання статистики
std::string stats;
Logger::getStatistics(stats);
// Виводить: кількість логів по рівням, використання місця тощо

// Перевірка використання місця
size_t used = Logger::getUsedSpace();
size_t total = Logger::getTotalSpace();
float percent = (float)used / total * 100;
MLOG_INFO("Log storage: %.1f%% used", percent);
```

## Обслуговування

### Автоматична ротація
- Відбувається автоматично при досягненні 64KB
- current.log → archive_1.log → archive_2.log → archive_3.log
- Найстаріші файли видаляються при переповненні

### Ручне очищення
```cpp
// Очистити всі логи
Logger::clearLogs();

// Очистити старі логи
LogFilter filter;
filter.endTime = timestamp - (7 * 24 * 3600);  // Старше 7 днів
Logger::clearLogs(filter);
```

## Переваги LittleFS над SPIFFS

1. **Wear leveling** - рівномірний знос flash пам'яті
2. **Швидкість** - кращі показники для малих файлів
3. **Надійність** - стійкість до втрати живлення
4. **Ефективність** - менше накладних витрат
5. **Сумісність** - краща підтримка в ESP-IDF

## Міграція з SPIFFS на LittleFS

Для міграції існуючих проектів:
1. Змінити тип партиції в partitions.csv на 0x82
2. Оновити залежності в CMakeLists.txt
3. Замінити API виклики esp_spiffs на esp_littlefs
4. Перекомпілювати та перепрошити пристрій

## Відомі обмеження

- Максимальний розмір файлу: обмежений конфігурацією (64KB)
- Кількість файлів: залежить від розміру метаданих
- Швидкість запису: ~50-100 KB/s (залежить від flash)