#include "logger_module.h"
#include "shared_state.h"
#include "event_bus.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstdarg>
#include <cstring>

static const char* TAG = "LoggerModule";

namespace ModESP {

LoggerModule::LoggerModule() 
    : m_ramBuffer(RAM_BUFFER_SIZE)
    , m_writeQueue(nullptr)
    , m_writerTaskHandle(nullptr)
    , m_nextModuleId(1)
    , m_initialized(false)
    , m_running(false) {
}

LoggerModule::~LoggerModule() {
    stop();
}

void LoggerModule::configure(const nlohmann::json& config) {
    if (!config.empty()) {
        m_config.enabled = config.value("enabled", true);
        
        std::string level = config.value("level", "INFO");
        m_config.defaultLevel = 
            level == "DEBUG" ? LogLevel::DEBUG :
            level == "INFO" ? LogLevel::INFO :
            level == "WARNING" ? LogLevel::WARNING :
            level == "ERROR" ? LogLevel::ERROR : LogLevel::CRITICAL;
            
        if (config.contains("storage")) {
            auto storage = config["storage"];
            m_config.maxFileSize = storage.value("maxFileSize", 1024*1024);
            m_config.maxTotalSize = storage.value("maxTotalSize", 10*1024*1024);
            m_config.maxArchiveFiles = storage.value("maxArchiveFiles", 5);
            m_config.flushInterval = storage.value("flushInterval", 5000);
        }
    }
}

esp_err_t LoggerModule::init() {
    ESP_LOGI(TAG, "Initializing LoggerModule");
    
    if (!m_config.enabled) {
        ESP_LOGI(TAG, "Logger disabled in configuration");
        return ESP_OK;
    }
    
    // Створюємо чергу для асинхронного запису
    m_writeQueue = xQueueCreate(WRITE_QUEUE_SIZE, sizeof(LogEntry));
    if (!m_writeQueue) {
        ESP_LOGE(TAG, "Failed to create write queue");
        return ESP_FAIL;
    }
    
    // Ініціалізуємо сховище
    m_storage = std::make_unique<LogStorage>(m_config);
    esp_err_t ret = m_storage->init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize storage: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Реєструємо стандартні модулі
    m_moduleMap["SYSTEM"] = m_nextModuleId++;
    m_moduleMap["SENSOR"] = m_nextModuleId++;
    m_moduleMap["ACTUATOR"] = m_nextModuleId++;
    m_moduleMap["DEFROST"] = m_nextModuleId++;
    m_moduleMap["COMPRESSOR"] = m_nextModuleId++;
    m_moduleMap["UI"] = m_nextModuleId++;
    m_moduleMap["NETWORK"] = m_nextModuleId++;
    
    // Ініціалізуємо статистику
    m_stats = {};
    
    m_initialized = true;
    m_running = true;  // Встановлюємо перед створенням задачі
    
    // Створюємо задачу для асинхронного запису
    BaseType_t task_ret = xTaskCreate(
        writerTaskWrapper,
        "logger_writer",
        WRITER_TASK_STACK,
        this,
        WRITER_TASK_PRIORITY,
        &m_writerTaskHandle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create writer task");
        m_running = false;
        return ESP_FAIL;
    }
    
    // Підписуємось на події
    setupEventSubscriptions();
    
    // Логуємо перше повідомлення
    logEvent(EventCode::SYSTEM_START, esp_timer_get_time() / 1000);
    
    ESP_LOGI(TAG, "LoggerModule initialized successfully");
    return ESP_OK;
}

void LoggerModule::stop() {
    if (!m_running) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping LoggerModule");
    
    // Зупиняємо робочу задачу
    m_running = false;
    
    // Відправляємо стоп-сигнал
    if (m_writerTaskHandle) {
        LogEntry stopEntry = {};
        stopEntry.level = static_cast<uint8_t>(LogLevel::NONE);
        xQueueSend(m_writeQueue, &stopEntry, portMAX_DELAY);
        
        // Очікуємо завершення задачі (вона видалить себе)
        vTaskDelay(pdMS_TO_TICKS(200));
        m_writerTaskHandle = nullptr;
    }
    
    // Записуємо залишки буферу
    if (m_storage) {
        auto entries = m_ramBuffer.getAll();
        for (const auto& entry : entries) {
            m_storage->writeEntry(entry);
        }
    }
    
    ESP_LOGI(TAG, "LoggerModule stopped");
}

void LoggerModule::logFormatted(LogLevel level, const char* module, const char* message) {
    if (!m_initialized || !m_config.enabled) {
        return;
    }
    
    if (level < m_config.defaultLevel) {
        return;
    }
    
    LogEntry entry = {};
    entry.timestamp = esp_timer_get_time() / 1000;
    entry.level = static_cast<uint8_t>(level);
    entry.moduleId = getModuleId(module);
    
    // Копіюємо повідомлення
    strncpy(entry.message, message, sizeof(entry.message) - 1);
    
    // Додаємо в буфер
    m_ramBuffer.push(entry);
    
    // Оновлюємо статистику
    m_stats.totalLogs++;
    m_stats.levelCounts[level]++;
    
    // Відправляємо в чергу запису
    if (m_writeQueue) {
        xQueueSend(m_writeQueue, &entry, 0);
    }
}

void LoggerModule::log(LogLevel level, const char* module, const char* fmt, ...) {
    // Форматуємо повідомлення
    char buffer[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Викликаємо основну функцію
    logFormatted(level, module, buffer);
}

void LoggerModule::logEvent(EventCode code, int32_t value, const char* message) {
    if (!m_initialized || !m_config.enabled) {
        return;
    }
    
    LogEntry entry = {};
    entry.timestamp = esp_timer_get_time() / 1000;
    entry.level = static_cast<uint8_t>(LogLevel::INFO);
    entry.moduleId = getModuleId("SYSTEM");
    entry.eventCode = static_cast<uint16_t>(code);
    entry.value = value;
    
    if (message) {
        strncpy(entry.message, message, sizeof(entry.message) - 1);
    }
    
    // Додаємо в буфер
    m_ramBuffer.push(entry);
    
    // Оновлюємо статистику
    m_stats.totalLogs++;
    m_stats.eventLogs++;
    
    // Відправляємо в чергу запису
    if (m_writeQueue) {
        xQueueSend(m_writeQueue, &entry, 0);
    }
}

void LoggerModule::logSensorData(uint8_t sensorId, float value) {
    char msg[32];
    snprintf(msg, sizeof(msg), "Sensor %d: %.2f", sensorId, value);
    logEvent(EventCode::TEMP_SNAPSHOT, static_cast<int32_t>(value * 100), msg);
}

void LoggerModule::logCompressorCycle(bool on, uint32_t runtime) {
    EventCode code = on ? EventCode::COMPRESSOR_ON : EventCode::COMPRESSOR_OFF;
    char msg[64];
    snprintf(msg, sizeof(msg), "Runtime: %lu seconds", runtime);
    logEvent(code, runtime, msg);
}

void LoggerModule::logDefrostCycle(uint8_t phase, int duration) {
    EventCode code;
    switch (phase) {
        case 0: code = EventCode::DEFROST_START; break;
        case 1: code = EventCode::DEFROST_END; break;
        case 2: code = EventCode::DEFROST_TIMEOUT; break;
        default: code = EventCode::DEFROST_SKIP; break;
    }
    
    char msg[64];
    snprintf(msg, sizeof(msg), "Phase: %d, Duration: %d sec", phase, duration);
    logEvent(code, duration, msg);
}

void LoggerModule::logHACCPEvent(EventCode type, float temperature) {
    const char* typeStr;
    switch (type) {
        case EventCode::HACCP_TEMP_VIOLATION:
            typeStr = "Temperature violation";
            break;
        case EventCode::HACCP_TEMP_RESTORED:
            typeStr = "Temperature restored";
            break;
        case EventCode::DOOR_ALARM:
            typeStr = "Door open alarm";
            break;
        default:
            typeStr = "HACCP event";
    }
    
    char msg[128];
    snprintf(msg, sizeof(msg), "%s: %.1f°C", typeStr, temperature);
    logEvent(type, static_cast<int32_t>(temperature * 10), msg);
}

size_t LoggerModule::getLogCount() {
    if (!m_storage) {
        return m_ramBuffer.size();
    }
    return m_storage->getLogCount() + m_ramBuffer.size();
}

std::vector<LogEntry> LoggerModule::readLogs(const LogFilter& filter) {
    std::vector<LogEntry> result;
    
    // Читаємо з файлу
    if (m_storage) {
        result = m_storage->readLogs(filter);
    }
    
    // Додаємо з RAM буферу
    auto ramLogs = m_ramBuffer.getAll();
    for (const auto& entry : ramLogs) {
        // Застосовуємо фільтр
        if (static_cast<LogLevel>(entry.level) < filter.minLevel) continue;
        if (filter.startTime > 0 && entry.timestamp < filter.startTime) continue;
        if (filter.endTime > 0 && entry.timestamp > filter.endTime) continue;
        
        if (!filter.moduleIds.empty()) {
            if (std::find(filter.moduleIds.begin(), filter.moduleIds.end(),
                entry.moduleId) == filter.moduleIds.end()) continue;
        }
        
        if (!filter.eventCodes.empty()) {
            if (std::find(filter.eventCodes.begin(), filter.eventCodes.end(),
                entry.eventCode) == filter.eventCodes.end()) continue;
        }
        
        result.push_back(entry);
        
        if (result.size() >= filter.maxEntries) {
            break;
        }
    }
    
    return result;
}

bool LoggerModule::clearLogs() {
    ESP_LOGI(TAG, "Clearing all logs");
    
    m_ramBuffer.clear();
    
    if (m_storage) {
        return m_storage->clearLogs();
    }
    
    return true;
}

bool LoggerModule::exportLogs(const std::string& format, std::string& output) {
    if (!m_storage) {
        return false;
    }
    
    LogFilter filter; // Без фільтрації
    
    if (format == "csv") {
        return m_storage->exportToCSV(output, filter);
    } else if (format == "json") {
        return m_storage->exportToJSON(output, filter);
    }
    
    return false;
}

size_t LoggerModule::getUsedSpace() {
    if (!m_storage) {
        return 0;
    }
    return m_storage->getUsedSpace();
}

size_t LoggerModule::getTotalSpace() {
    return m_config.maxTotalSize;
}

void LoggerModule::getStatistics(std::string& stats) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer),
        "Logger Statistics:\n"
        "Total logs: %lu\n"
        "Event logs: %lu\n"
        "Warnings: %lu\n"
        "Errors: %lu\n"
        "Rotations: %lu\n"
        "RAM buffer: %zu/%zu\n"
        "Level counts:\n"
        "  DEBUG: %lu\n"
        "  INFO: %lu\n"
        "  WARNING: %lu\n"
        "  ERROR: %lu\n"
        "  CRITICAL: %lu\n",
        m_stats.totalLogs,
        m_stats.eventLogs,
        m_stats.warnings,
        m_stats.errors,
        m_stats.rotations,
        m_ramBuffer.size(), RAM_BUFFER_SIZE,
        m_stats.levelCounts[LogLevel::DEBUG],
        m_stats.levelCounts[LogLevel::INFO],
        m_stats.levelCounts[LogLevel::WARNING],
        m_stats.levelCounts[LogLevel::ERROR],
        m_stats.levelCounts[LogLevel::CRITICAL]
    );
    
    stats = buffer;
}

uint8_t LoggerModule::getModuleId(const char* moduleName) {
    std::string name(moduleName);
    
    auto it = m_moduleMap.find(name);
    if (it != m_moduleMap.end()) {
        return it->second;
    }
    
    // Новий модуль - додаємо
    uint8_t id = m_nextModuleId++;
    m_moduleMap[name] = id;
    return id;
}

void LoggerModule::setupEventSubscriptions() {
    // Підписуємось на температурні аларми
    EventBus::subscribe("sensor.temperature.alarm", [this](const EventBus::Event& e) {
        float temp = e.data.value("temperature", 0.0f);
        logHACCPEvent(EventCode::HACCP_TEMP_VIOLATION, temp);
    });
    
    // Підписуємось на стан компресора
    EventBus::subscribe("actuator.compressor.state", [this](const EventBus::Event& e) {
        bool on = e.data.value("on", false);
        uint32_t runtime = e.data.value("runtime", 0);
        logCompressorCycle(on, runtime);
    });
}

void LoggerModule::writerTaskWrapper(void* param) {
    static_cast<LoggerModule*>(param)->writerTask();
}

void LoggerModule::writerTask() {
    ESP_LOGI(TAG, "Writer task started");
    
    LogEntry entry;
    std::vector<LogEntry> batch;
    batch.reserve(10);
    
    TickType_t lastFlush = xTaskGetTickCount();
    
    while (m_running) {
        // Очікуємо дані
        if (xQueueReceive(m_writeQueue, &entry, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Перевіряємо стоп-сигнал
            if (entry.level == static_cast<uint8_t>(LogLevel::NONE)) {
                break;
            }
            
            batch.push_back(entry);
            
            // Записуємо пакетами
            if (batch.size() >= 10) {
                if (m_storage) {
                    m_storage->writeEntries(batch);
                }
                batch.clear();
            }
        }
        
        // Періодичний запис
        if (batch.size() > 0 &&
            (xTaskGetTickCount() - lastFlush) > pdMS_TO_TICKS(m_config.flushInterval)) {
            if (m_storage) {
                m_storage->writeEntries(batch);
            }
            batch.clear();
            lastFlush = xTaskGetTickCount();
        }
        
        // Перевіряємо ротацію
        checkRotation();
    }
    
    // Записуємо залишки
    if (!batch.empty() && m_storage) {
        m_storage->writeEntries(batch);
    }
    
    ESP_LOGI(TAG, "Writer task stopped");
    
    // Видаляємо задачу
    vTaskDelete(NULL);
}

void LoggerModule::checkRotation() {
    static TickType_t lastCheck = 0;
    TickType_t now = xTaskGetTickCount();
    
    // Перевіряємо раз на хвилину
    if ((now - lastCheck) < pdMS_TO_TICKS(60000)) {
        return;
    }
    
    lastCheck = now;
    
    // Ротація файлів
    if (m_storage && m_storage->rotateFiles()) {
        m_stats.rotations++;
        ESP_LOGI(TAG, "Log files rotated");
    }
}

} // namespace ModESP
