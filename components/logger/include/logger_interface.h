#pragma once

#include <cstdint>
#include <string>
#include <functional>
#include <vector>

namespace ModESP {

/**
 * @brief Рівні логування
 */
enum class LogLevel : uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4,
    NONE = 5
};

/**
 * @brief Коди подій для структурованого логування
 */
enum class EventCode : uint16_t {
    // Загальні події (0x0000 - 0x00FF)
    SYSTEM_START = 0x0001,
    SYSTEM_STOP = 0x0002,
    CONFIG_CHANGED = 0x0003,
    MODULE_ERROR = 0x0004,
    
    // Температурні події (0x0100 - 0x01FF)
    TEMP_ALARM_HIGH = 0x0100,
    TEMP_ALARM_LOW = 0x0101,
    TEMP_NORMAL = 0x0102,
    TEMP_SENSOR_FAIL = 0x0103,
    TEMP_SNAPSHOT = 0x0104,
    
    // Події розморозки (0x0200 - 0x02FF)
    DEFROST_START = 0x0200,
    DEFROST_END = 0x0201,
    DEFROST_TIMEOUT = 0x0202,
    DEFROST_SKIP = 0x0203,
    
    // Події компресора (0x0300 - 0x03FF)
    COMPRESSOR_ON = 0x0300,
    COMPRESSOR_OFF = 0x0301,
    COMPRESSOR_PROTECT = 0x0302,
    COMPRESSOR_STATS = 0x0303,
    
    // Події дверей (0x0400 - 0x04FF)
    DOOR_OPEN = 0x0400,
    DOOR_CLOSE = 0x0401,
    DOOR_ALARM = 0x0402,
    
    // HACCP події (0x0500 - 0x05FF)
    HACCP_TEMP_VIOLATION = 0x0500,
    HACCP_TEMP_RESTORED = 0x0501,
    HACCP_REPORT = 0x0502
};

/**
 * @brief Структура запису логу
 */
struct LogEntry {
    uint32_t timestamp;      // Unix timestamp
    uint16_t milliseconds;   // Мілісекунди
    uint8_t level;          // LogLevel
    uint8_t moduleId;       // ID модуля
    uint16_t eventCode;     // EventCode
    int32_t value;          // Числове значення
    char message[32];       // Коротке повідомлення
} __attribute__((packed));

/**
 * @brief Фільтр для читання логів
 */
struct LogFilter {
    LogLevel minLevel = LogLevel::DEBUG;
    uint32_t startTime = 0;
    uint32_t endTime = 0;
    std::vector<uint8_t> moduleIds;
    std::vector<uint16_t> eventCodes;
    uint32_t maxEntries = 100;
};

/**
 * @brief Конфігурація логера
 */
struct LoggerConfig {
    bool enabled = true;
    LogLevel defaultLevel = LogLevel::INFO;
    size_t maxFileSize = 64 * 1024;      // 64KB на файл
    size_t maxTotalSize = 256 * 1024;    // 256KB всього
    uint8_t maxArchiveFiles = 3;         // 3 архівні файли
    uint32_t flushInterval = 5000;       // Запис кожні 5 сек
    bool compressOldLogs = false;        // Стиснення архівів
    bool haccp = true;                   // HACCP режим
};

/**
 * @brief Інтерфейс логера
 */
class ILogger {
public:
    virtual ~ILogger() = default;
    
    // Базові методи логування
    virtual void log(LogLevel level, const char* module, const char* fmt, ...) = 0;
    virtual void logEvent(EventCode code, int32_t value = 0, const char* message = nullptr) = 0;
    
    // Спеціалізовані методи
    virtual void logSensorData(uint8_t sensorId, float value) = 0;
    virtual void logCompressorCycle(bool on, uint32_t runtime) = 0;
    virtual void logDefrostCycle(uint8_t phase, int duration) = 0;
    virtual void logHACCPEvent(EventCode type, float temperature) = 0;
    
    // Управління логами
    virtual size_t getLogCount() = 0;
    virtual std::vector<LogEntry> readLogs(const LogFilter& filter) = 0;
    virtual bool clearLogs() = 0;
    virtual bool exportLogs(const std::string& format, std::string& output) = 0;
    
    // Статистика
    virtual size_t getUsedSpace() = 0;
    virtual size_t getTotalSpace() = 0;
    virtual void getStatistics(std::string& stats) = 0;
};

// Глобальний доступ до логера
extern ILogger* g_logger;

// Макроси для зручного логування
#define MLOG_DEBUG(fmt, ...) \
    if (g_logger) g_logger->log(LogLevel::DEBUG, TAG, fmt, ##__VA_ARGS__)

#define MLOG_INFO(fmt, ...) \
    if (g_logger) g_logger->log(LogLevel::INFO, TAG, fmt, ##__VA_ARGS__)

#define MLOG_WARNING(fmt, ...) \
    if (g_logger) g_logger->log(LogLevel::WARNING, TAG, fmt, ##__VA_ARGS__)

#define MLOG_ERROR(fmt, ...) \
    if (g_logger) g_logger->log(LogLevel::ERROR, TAG, fmt, ##__VA_ARGS__)

#define MLOG_CRITICAL(fmt, ...) \
    if (g_logger) g_logger->log(LogLevel::CRITICAL, TAG, fmt, ##__VA_ARGS__)

#define MLOG_EVENT(code, value) \
    if (g_logger) g_logger->logEvent(code, value)

} // namespace ModESP
