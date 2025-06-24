#pragma once

#include "logger_interface.h"
#include <string>
#include <vector>
#include <esp_littlefs.h>

namespace ModESP {

/**
 * @brief Клас для роботи з файловим сховищем логів
 * 
 * Використовує LittleFS для збереження логів з підтримкою:
 * - Автоматичної ротації файлів
 * - Стиснення архівів
 * - Пошуку та фільтрації
 */
class LogStorage {
public:
    LogStorage(const LoggerConfig& config);
    ~LogStorage();
    
    // Ініціалізація та монтування LittleFS
    esp_err_t init();
    esp_err_t deinit();
    
    // Запис логів
    esp_err_t writeEntry(const LogEntry& entry);
    esp_err_t writeEntries(const std::vector<LogEntry>& entries);
    
    // Читання логів
    std::vector<LogEntry> readLogs(const LogFilter& filter);
    size_t getLogCount();
    
    // Управління файлами
    bool clearLogs();
    bool rotateFiles();
    
    // Експорт
    bool exportToCSV(std::string& output, const LogFilter& filter);
    bool exportToJSON(std::string& output, const LogFilter& filter);    
    // Статистика
    size_t getUsedSpace();
    size_t getTotalSpace();
    
private:
    // Внутрішні методи
    esp_err_t mountLittleFS();
    esp_err_t unmountLittleFS();
    
    std::string getCurrentLogPath();
    std::string getArchiveLogPath(int index);
    std::string getCriticalLogPath();
    
    bool checkRotationNeeded();
    void performRotation();
    void enforceQuota();
    
    size_t getFileSize(const std::string& path);
    bool deleteFile(const std::string& path);
    bool renameFile(const std::string& from, const std::string& to);
    
    // Робота з файлами
    FILE* openLogFile(const std::string& path, const char* mode);
    void closeLogFile(FILE* file);
    
    // Конфігурація
    const LoggerConfig& m_config;
    
    // Стан
    bool m_mounted;
    FILE* m_currentFile;
    size_t m_currentFileSize;
    
    // Шляхи
    static constexpr const char* MOUNT_POINT = "/logs";
    static constexpr const char* CURRENT_LOG = "/logs/current.log";
    static constexpr const char* CRITICAL_LOG = "/logs/critical.log";
    static constexpr const char* ARCHIVE_PREFIX = "/logs/archive_";
};

} // namespace ModESP
