#include "logger_module.h"
#include "core/shared_state.h"
#include "core/event_bus.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <cstdarg>
#include <cstring>

static const char* TAG = "LoggerModule";

namespace ModESP {

// Глобальний екземпляр логера
ILogger* g_logger = nullptr;

LoggerModule::LoggerModule() 
    : m_ramBuffer(RAM_BUFFER_SIZE)
    , m_nextModuleId(0)
    , m_initialized(false)
    , m_running(false) {
    
    // Реєструємо себе як глобальний логер
    g_logger = this;
}

LoggerModule::~LoggerModule() {
    if (g_logger == this) {
        g_logger = nullptr;
    }
}

esp_err_t LoggerModule::onInit() {
    ESP_LOGI(TAG, "Initializing LoggerModule v%s", version());
    
    // Завантажуємо конфігурацію
    auto config = m_configManager->getComponentConfig(name());
    if (!config.empty()) {
        m_config.enabled = config["enabled"].get<bool>();
        m_config.defaultLevel = static_cast<LogLevel>(
            config["level"].get<std::string>() == "DEBUG" ? 0 :
            config["level"].get<std::string>() == "INFO" ? 1 :
            config["level"].get<std::string>() == "WARNING" ? 2 :
            config["level"].get<std::string>() == "ERROR" ? 3 : 4
        );
    }        
        if (config.contains("storage")) {
            auto storage = config["storage"];
            m_config.maxFileSize = storage["maxFileSize"].get<size_t>();
            m_config.maxTotalSize = storage["maxTotalSize"].get<size_t>();
            m_config.maxArchiveFiles = storage["maxArchiveFiles"].get<uint8_t>();
            m_config.flushInterval = storage["flushInterval"].get<uint32_t>();
        }
    }
    
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
    
    // Логуємо перше повідомлення
    logEvent(EventCode::SYSTEM_START, esp_timer_get_time() / 1000);
    
    ESP_LOGI(TAG, "LoggerModule initialized successfully");
    return ESP_OK;
}

esp_err_t LoggerModule::onStart() {
    if (!m_initialized || !m_config.enabled) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting LoggerModule");
    
    // Створюємо задачу для асинхронного запису
    BaseType_t ret = xTaskCreate(
        writerTaskWrapper,
        "logger_writer",
        WRITER_TASK_STACK,
        this,
        WRITER_TASK_PRIORITY,
        &m_writerTaskHandle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create writer task");
        return ESP_FAIL;
    }
    
    // Підписуємось на події
    setupEventSubscriptions();
    
    m_running = true;
    
    ESP_LOGI(TAG, "LoggerModule started");
    return ESP_OK;
}
esp_err_t LoggerModule::onStop() {
    if (!m_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping LoggerModule");
    
    m_running = false;
    
    // Завершуємо задачу
    if (m_writerTaskHandle) {
        // Надсилаємо спеціальний маркер для завершення
        LogEntry stopEntry = {};
        stopEntry.level = static_cast<uint8_t>(LogLevel::NONE);
        xQueueSend(m_writeQueue, &stopEntry, portMAX_DELAY);
        
        // Чекаємо завершення задачі
        vTaskDelay(pdMS_TO_TICKS(100));
        vTaskDelete(m_writerTaskHandle);
        m_writerTaskHandle = nullptr;
    }
    
    // Записуємо останні логи з буфера
    if (m_storage) {
        auto entries = m_ramBuffer.getAll();
        if (!entries.empty()) {
            m_storage->writeEntries(entries);
        }
    }
    
    ESP_LOGI(TAG, "LoggerModule stopped");
    return ESP_OK;
}

esp_err_t LoggerModule::onReset() {
    ESP_LOGI(TAG, "Resetting LoggerModule");
    
    // Очищуємо RAM буфер
    m_ramBuffer.clear();
    
    // Очищуємо статистику
    m_stats = {};
    
    // Очищуємо файли логів якщо потрібно
    if (m_storage) {
        m_storage->clearLogs();
    }
    
    ESP_LOGI(TAG, "LoggerModule reset complete");
    return ESP_OK;
}
void LoggerModule::log(LogLevel level, const char* module, const char* fmt, ...) {
    if (!m_initialized || !m_config.enabled || level < m_config.defaultLevel) {
        return;
    }
    
    LogEntry entry = {};
    entry.timestamp = esp_timer_get_time() / 1000000; // Конвертуємо в секунди
    entry.milliseconds = (esp_timer_get_time() / 1000) % 1000;
    entry.level = static_cast<uint8_t>(level);
    entry.moduleId = getModuleId(module);
    entry.eventCode = 0; // Звичайне повідомлення
    entry.value = 0;
    
    // Форматуємо повідомлення
    va_list args;
    va_start(args, fmt);
    vsnprintf(entry.message, sizeof(entry.message), fmt, args);
    va_end(args);
    
    // Додаємо в RAM буфер
    m_ramBuffer.push(entry);
    
    // Відправляємо в чергу для запису
    if (m_writeQueue) {
        xQueueSend(m_writeQueue, &entry, 0);
    }
    
    // Оновлюємо статистику
    m_stats.totalLogs++;
    m_stats.levelCounts[level]++;
}
void LoggerModule::logEvent(EventCode code, int32_t value, const char* message) {
    if (!m_initialized || !m_config.enabled) {
        return;
    }
    
    LogEntry entry = {};
    entry.timestamp = esp_timer_get_time() / 1000000;
    entry.milliseconds = (esp_timer_get_time() / 1000) % 1000;
    entry.level = static_cast<uint8_t>(LogLevel::INFO);
    entry.moduleId = 0; // SYSTEM module
    entry.eventCode = static_cast<uint16_t>(code);
    entry.value = value;
    
    if (message) {
        strncpy(entry.message, message, sizeof(entry.message) - 1);
    }
    
    // Визначаємо рівень логування за кодом події
    if (code >= EventCode::TEMP_ALARM_HIGH && code <= EventCode::TEMP_ALARM_LOW) {
        entry.level = static_cast<uint8_t>(LogLevel::WARNING);
    } else if (code == EventCode::MODULE_ERROR || code == EventCode::TEMP_SENSOR_FAIL) {
        entry.level = static_cast<uint8_t>(LogLevel::ERROR);
    } else if (code >= EventCode::HACCP_TEMP_VIOLATION) {
        entry.level = static_cast<uint8_t>(LogLevel::CRITICAL);
    }
    
    // Додаємо в буфери
    m_ramBuffer.push(entry);
    if (m_writeQueue) {
        xQueueSend(m_writeQueue, &entry, 0);
    }
    
    // Статистика
    m_stats.totalLogs++;
    m_stats.eventCounts[static_cast<uint16_t>(code)]++;
}
void LoggerModule::logSensorData(uint8_t sensorId, float value) {
    char msg[32];
    snprintf(msg, sizeof(msg), "Sensor%d: %.2f", sensorId, value);
    logEvent(EventCode::TEMP_SNAPSHOT, static_cast<int32_t>(value * 100), msg);
}

void LoggerModule::logCompressorCycle(bool on, uint32_t runtime) {
    EventCode code = on ? EventCode::COMPRESSOR_ON : EventCode::COMPRESSOR_OFF;
    char msg[32];
    snprintf(msg, sizeof(msg), "Runtime: %lu sec", runtime);
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
    
    char msg[32];
    snprintf(msg, sizeof(msg), "Duration: %d min", duration);
    logEvent(code, duration, msg);
}

void LoggerModule::logHACCPEvent(EventCode type, float temperature) {
    LogEntry entry = {};
    entry.timestamp = esp_timer_get_time() / 1000000;
    entry.milliseconds = (esp_timer_get_time() / 1000) % 1000;
    entry.level = static_cast<uint8_t>(LogLevel::CRITICAL);
    entry.moduleId = getModuleId("HACCP");
    entry.eventCode = static_cast<uint16_t>(type);
    entry.value = static_cast<int32_t>(temperature * 100);
    
    snprintf(entry.message, sizeof(entry.message), "HACCP: %.1f°C", temperature);
    
    // HACCP події завжди записуються одразу
    m_ramBuffer.push(entry);
    if (m_storage) {
        m_storage->writeEntry(entry);
    }
    
    m_stats.totalLogs++;
    m_stats.eventCounts[static_cast<uint16_t>(type)]++;
}
size_t LoggerModule::getLogCount() {
    if (!m_storage) {
        return m_ramBuffer.size();
    }
    return m_storage->getLogCount() + m_ramBuffer.size();
}

std::vector<LogEntry> LoggerModule::readLogs(const LogFilter& filter) {
    std::vector<LogEntry> result;
    
    // Спочатку читаємо з файлів
    if (m_storage) {
        result = m_storage->readLogs(filter);
    }
    
    // Додаємо з RAM буфера
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
        "Dropped logs: %lu\n"
        "File rotations: %lu\n"
        "Used space: %zu / %zu bytes (%.1f%%)\n"
        "RAM buffer: %zu / %zu entries\n\n"
        "Log levels:\n"
        "  DEBUG: %lu\n"
        "  INFO: %lu\n"
        "  WARNING: %lu\n"
        "  ERROR: %lu\n"
        "  CRITICAL: %lu\n",
        m_stats.totalLogs,
        m_stats.droppedLogs,
        m_stats.rotations,
        getUsedSpace(), getTotalSpace(),
        (float)getUsedSpace() * 100 / getTotalSpace(),
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
    
    // Реєструємо новий модуль
    uint8_t id = m_nextModuleId++;
    m_moduleMap[name] = id;
    return id;
}

void LoggerModule::setupEventSubscriptions() {
    if (!m_eventBus) {
        return;
    }
    
    // Підписка на температурні події
    m_eventBus->subscribe("sensor.temperature.alarm", [this](const Event& e) {
        float temp = e.data["temperature"].get<float>();
        bool high = e.data["high"].get<bool>();
        logEvent(high ? EventCode::TEMP_ALARM_HIGH : EventCode::TEMP_ALARM_LOW, 
                 static_cast<int32_t>(temp * 100));
    });
    
    // Підписка на події компресора
    m_eventBus->subscribe("actuator.compressor.state", [this](const Event& e) {
        bool on = e.data["on"].get<bool>();
        uint32_t runtime = e.data["runtime"].get<uint32_t>();
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
    TickType_t lastFlush = xTaskGetTickCount();
    
    while (m_running) {
        // Чекаємо на нові записи
        if (xQueueReceive(m_writeQueue, &entry, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Перевіряємо маркер завершення
            if (entry.level == static_cast<uint8_t>(LogLevel::NONE)) {
                break;
            }
            
            batch.push_back(entry);
            
            // Записуємо пачками
            if (batch.size() >= 10) {
                if (m_storage) {
                    m_storage->writeEntries(batch);
                }
                batch.clear();
                lastFlush = xTaskGetTickCount();
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
        
        // Перевіряємо необхідність ротації
        checkRotation();
    }
    
    // Записуємо залишки
    if (!batch.empty() && m_storage) {
        m_storage->writeEntries(batch);
    }
    
    ESP_LOGI(TAG, "Writer task stopped");
}

void LoggerModule::checkRotation() {
    static TickType_t lastCheck = 0;
    TickType_t now = xTaskGetTickCount();
    
    // Перевіряємо раз на хвилину
    if ((now - lastCheck) < pdMS_TO_TICKS(60000)) {
        return;
    }
    
    lastCheck = now;
    
    if (m_storage && m_storage->rotateFiles()) {
        m_stats.rotations++;
        ESP_LOGI(TAG, "Log files rotated");
    }
}

} // namespace ModESP
