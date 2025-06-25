#pragma once

#include "base_module.h"
#include "logger_interface.h"
#include "ring_buffer.h"
#include "log_storage.h"
#include "event_bus.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <memory>
#include <map>

namespace ModESP {

/**
 * @brief Модуль логування для ModuChill
 * 
 * Забезпечує збереження та управління логами з підтримкою:
 * - LittleFS для постійного зберігання
 * - Кільцевого буфера в RAM для швидкого доступу
 * - Асинхронного запису для мінімального впливу на систему
 * - Автоматичної ротації файлів
 * - HACCP сумісності
 */
class LoggerModule : public BaseModule, public ILogger {
public:
    LoggerModule();
    ~LoggerModule();
    
    // BaseModule interface
    const char* get_name() const override { return "LoggerModule"; }
    esp_err_t init() override;
    void update() override {} // Logger doesn't need periodic updates
    void stop() override;
    void configure(const nlohmann::json& config) override;
        
    // ILogger interface
    void logFormatted(LogLevel level, const char* module, const char* message) override;
    void log(LogLevel level, const char* module, const char* fmt, ...);  // Non-virtual helper
    void logEvent(EventCode code, int32_t value = 0, const char* message = nullptr) override;
    
    void logSensorData(uint8_t sensorId, float value) override;
    void logCompressorCycle(bool on, uint32_t runtime) override;
    void logDefrostCycle(uint8_t phase, int duration) override;
    void logHACCPEvent(EventCode type, float temperature) override;
    
    size_t getLogCount() override;
    std::vector<LogEntry> readLogs(const LogFilter& filter) override;
    bool clearLogs() override;
    bool exportLogs(const std::string& format, std::string& output) override;
    
    size_t getUsedSpace() override;
    size_t getTotalSpace() override;
    void getStatistics(std::string& stats) override;
    
private:
    // Внутрішні методи
    void writerTask();
    void processLogEntry(const LogEntry& entry);
    void checkRotation();
    uint8_t getModuleId(const char* moduleName);
    void setupEventSubscriptions();
    
    // Конфігурація
    LoggerConfig m_config;
    
    // Кільцевий буфер в RAM
    RingBuffer<LogEntry> m_ramBuffer;    
    // Зберігання логів
    std::unique_ptr<LogStorage> m_storage;
    
    // Асинхронний запис
    QueueHandle_t m_writeQueue;
    TaskHandle_t m_writerTaskHandle;
    
    // Статистика
    struct {
        uint32_t totalLogs;
        uint32_t eventLogs;
        uint32_t warnings;
        uint32_t errors;
        uint32_t droppedLogs;
        uint32_t rotations;
        std::map<LogLevel, uint32_t> levelCounts;
        std::map<uint16_t, uint32_t> eventCounts;
    } m_stats;
    
    // Мапа модулів
    std::map<std::string, uint8_t> m_moduleMap;
    uint8_t m_nextModuleId;
    
    // Прапорці стану
    bool m_initialized;
    bool m_running;
    
    // Константи
    static constexpr size_t WRITE_QUEUE_SIZE = 50;
    static constexpr size_t WRITER_TASK_STACK = 4096;
    static constexpr uint8_t WRITER_TASK_PRIORITY = 2;
    static constexpr size_t RAM_BUFFER_SIZE = 256;
    
    // Статична задача для FreeRTOS
    static void writerTaskWrapper(void* param);
};

} // namespace ModESP
