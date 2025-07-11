#include "log_storage.h"
#include <esp_log.h>
#include <esp_littlefs.h>
#include <cstring>
#include <algorithm>
#include <sys/stat.h>
#include <unistd.h>

static const char* TAG = "LogStorage";

namespace ModESP {

LogStorage::LogStorage(const LoggerConfig& config) 
    : m_config(config)
    , m_mounted(false)
    , m_currentFile(nullptr)
    , m_currentFileSize(0) {
}

LogStorage::~LogStorage() {
    deinit();
}

esp_err_t LogStorage::init() {
    ESP_LOGI(TAG, "Initializing LittleFS storage");
    
    // Монтуємо файлову систему
    esp_err_t ret = mountLittleFS();
    if (ret != ESP_OK) {
        return ret;
    }
    
    // Створюємо директорію для логів якщо не існує
    struct stat st;
    if (stat(MOUNT_POINT, &st) != 0) {
        ESP_LOGE(TAG, "Mount point doesn't exist");
        return ESP_FAIL;
    }
    
    // Створюємо директорію для логів
    if (stat(LOG_DIR, &st) != 0) {
        ESP_LOGI(TAG, "Creating logs directory: %s", LOG_DIR);
        if (mkdir(LOG_DIR, 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create logs directory");
            return ESP_FAIL;
        }
    }
    
    // Відкриваємо поточний файл логів
    m_currentFile = openLogFile(CURRENT_LOG, "ab");
    if (!m_currentFile) {
        ESP_LOGE(TAG, "Failed to open current log file");
        return ESP_FAIL;
    }
    
    // Визначаємо розмір файлу
    fseek(m_currentFile, 0, SEEK_END);
    m_currentFileSize = ftell(m_currentFile);
    
    ESP_LOGI(TAG, "Storage initialized, current log size: %zu bytes", m_currentFileSize);
    return ESP_OK;
}
esp_err_t LogStorage::deinit() {
    ESP_LOGI(TAG, "Deinitializing storage");
    
    if (m_currentFile) {
        closeLogFile(m_currentFile);
        m_currentFile = nullptr;
    }
    
    return unmountLittleFS();
}

esp_err_t LogStorage::mountLittleFS() {
    // Перевіряємо чи LittleFS вже змонтована (наприклад ConfigManager)
    struct stat st;
    if (stat(MOUNT_POINT, &st) == 0) {
        ESP_LOGI(TAG, "LittleFS already mounted at %s", MOUNT_POINT);
        m_mounted = true;
        
        // Виводимо інформацію про розділ
        size_t total = 0, used = 0;
        esp_err_t ret = esp_littlefs_info("storage", &total, &used);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
        }
        
        return ESP_OK;
    }
    
    // Якщо не змонтована, спробуємо змонтувати самостійно
    ESP_LOGI(TAG, "LittleFS not mounted, attempting to mount");
    esp_vfs_littlefs_conf_t conf = {
        .base_path = MOUNT_POINT,
        .partition_label = "storage",
        .partition = NULL,
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGI(TAG, "LittleFS already registered by another component");
            m_mounted = true;
            return ESP_OK;
        } else if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return ret;
    }
    
    m_mounted = true;
    ESP_LOGI(TAG, "LittleFS mounted successfully");
    
    // Виводимо інформацію про розділ
    size_t total = 0, used = 0;
    ret = esp_littlefs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    
    return ESP_OK;
}
esp_err_t LogStorage::unmountLittleFS() {
    if (!m_mounted) {
        return ESP_OK;
    }
    
    // Не розмонтовуємо файлову систему, оскільки її може використовувати ConfigManager
    // та інші компоненти. Залишаємо це на відповідальність головного компонента.
    ESP_LOGI(TAG, "Skipping LittleFS unmount - used by other components");
    
    m_mounted = false;
    return ESP_OK;
}

esp_err_t LogStorage::writeEntry(const LogEntry& entry) {
    if (!m_currentFile) {
        return ESP_FAIL;
    }
    
    // Записуємо запис
    size_t written = fwrite(&entry, sizeof(LogEntry), 1, m_currentFile);
    if (written != 1) {
        ESP_LOGE(TAG, "Failed to write log entry");
        return ESP_FAIL;
    }
    
    m_currentFileSize += sizeof(LogEntry);
    
    // Флашимо для критичних записів
    if (static_cast<LogLevel>(entry.level) >= LogLevel::ERROR) {
        fflush(m_currentFile);
    }
    
    // Перевіряємо необхідність ротації
    if (checkRotationNeeded()) {
        performRotation();
    }
    
    return ESP_OK;
}
esp_err_t LogStorage::writeEntries(const std::vector<LogEntry>& entries) {
    if (!m_currentFile || entries.empty()) {
        return ESP_FAIL;
    }
    
    // Записуємо всі записи
    size_t written = fwrite(entries.data(), sizeof(LogEntry), entries.size(), m_currentFile);
    if (written != entries.size()) {
        ESP_LOGE(TAG, "Failed to write all entries: %zu of %zu", written, entries.size());
        return ESP_FAIL;
    }
    
    m_currentFileSize += written * sizeof(LogEntry);
    
    // Перевіряємо ротацію
    if (checkRotationNeeded()) {
        performRotation();
    }
    
    return ESP_OK;
}

std::vector<LogEntry> LogStorage::readLogs(const LogFilter& filter) {
    std::vector<LogEntry> result;
    
    // Список файлів для читання
    std::vector<std::string> files = {
        CURRENT_LOG,
        getCriticalLogPath()
    };
    
    // Додаємо архівні файли
    for (int i = 1; i <= m_config.maxArchiveFiles; i++) {
        files.push_back(getArchiveLogPath(i));
    }    
    // Читаємо з кожного файлу
    for (const auto& filepath : files) {
        FILE* file = openLogFile(filepath, "rb");
        if (!file) continue;
        
        LogEntry entry;
        while (fread(&entry, sizeof(LogEntry), 1, file) == 1) {
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
                closeLogFile(file);
                return result;
            }
        }
        
        closeLogFile(file);
    }
    
    return result;
}
size_t LogStorage::getLogCount() {
    size_t count = 0;
    
    // Список файлів для підрахунку
    std::vector<std::string> files = {
        CURRENT_LOG,
        getCriticalLogPath()
    };
    
    // Додаємо архівні файли
    for (int i = 1; i <= m_config.maxArchiveFiles; i++) {
        files.push_back(getArchiveLogPath(i));
    }
    
    for (const auto& filepath : files) {
        FILE* file = openLogFile(filepath, "rb");
        if (!file) continue;
        
        // Рахуємо записи
        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        count += fileSize / sizeof(LogEntry);
        
        closeLogFile(file);
    }
    
    return count;
}

bool LogStorage::clearLogs() {
    ESP_LOGI(TAG, "Clearing all log files");
    
    // Закриваємо поточний файл
    if (m_currentFile) {
        closeLogFile(m_currentFile);
        m_currentFile = nullptr;
    }
    
    // Видаляємо всі файли
    deleteFile(CURRENT_LOG);
    deleteFile(getCriticalLogPath());
    
    for (int i = 1; i <= m_config.maxArchiveFiles; i++) {
        deleteFile(getArchiveLogPath(i));
    }
    
    // Відкриваємо новий поточний файл
    m_currentFile = openLogFile(CURRENT_LOG, "wb");
    m_currentFileSize = 0;
    
    return m_currentFile != nullptr;
}

bool LogStorage::rotateFiles() {
    if (!checkRotationNeeded()) {
        return false;
    }
    
    performRotation();
    return true;
}

bool LogStorage::exportToCSV(std::string& output, const LogFilter& filter) {
    auto logs = readLogs(filter);
    
    output = "Timestamp,Level,Module,Event,Value,Message\n";
    
    for (const auto& entry : logs) {
        char line[256];
        snprintf(line, sizeof(line), 
            "%lu,%u,%u,%u,%ld,%s\n",
            entry.timestamp,
            entry.level,
            entry.moduleId,
            entry.eventCode,
            entry.value,
            entry.message
        );
        output += line;
    }
    
    return true;
}

bool LogStorage::exportToJSON(std::string& output, const LogFilter& filter) {
    auto logs = readLogs(filter);
    
    output = "{\n  \"logs\": [\n";
    
    for (size_t i = 0; i < logs.size(); i++) {
        const auto& entry = logs[i];
        
        char jsonEntry[512];
        snprintf(jsonEntry, sizeof(jsonEntry),
            "    {\n"
            "      \"timestamp\": %lu,\n"
            "      \"level\": %u,\n"
            "      \"module\": %u,\n"
            "      \"event\": %u,\n"
            "      \"value\": %ld,\n"
            "      \"message\": \"%s\"\n"
            "    }%s\n",
            entry.timestamp,
            entry.level,
            entry.moduleId,
            entry.eventCode,
            entry.value,
            entry.message,
            (i < logs.size() - 1) ? "," : ""
        );
        
        output += jsonEntry;
    }
    
    output += "  ]\n}";
    return true;
}

size_t LogStorage::getUsedSpace() {
    size_t total = 0;
    
    // Поточний файл
    total += getFileSize(CURRENT_LOG);
    
    // Критичний файл
    total += getFileSize(getCriticalLogPath());
    
    // Архівні файли
    for (int i = 1; i <= m_config.maxArchiveFiles; i++) {
        total += getFileSize(getArchiveLogPath(i));
    }
    
    return total;
}

size_t LogStorage::getTotalSpace() {
    return m_config.maxTotalSize;
}

// Приватні методи

std::string LogStorage::getCurrentLogPath() {
    return CURRENT_LOG;
}

std::string LogStorage::getArchiveLogPath(int index) {
    return std::string(ARCHIVE_PREFIX) + std::to_string(index) + ".log";
}

std::string LogStorage::getCriticalLogPath() {
    return CRITICAL_LOG;
}

bool LogStorage::checkRotationNeeded() {
    return m_currentFileSize >= m_config.maxFileSize;
}

void LogStorage::performRotation() {
    ESP_LOGI(TAG, "Performing log rotation");
    
    // Закриваємо поточний файл
    if (m_currentFile) {
        closeLogFile(m_currentFile);
        m_currentFile = nullptr;
    }
    
    // Зсуваємо архівні файли
    for (int i = m_config.maxArchiveFiles; i > 1; i--) {
        std::string from = getArchiveLogPath(i - 1);
        std::string to = getArchiveLogPath(i);
        renameFile(from, to);
    }
    
    // Переміщуємо поточний в архів
    renameFile(CURRENT_LOG, getArchiveLogPath(1));
    
    // Створюємо новий поточний файл
    m_currentFile = openLogFile(CURRENT_LOG, "wb");
    m_currentFileSize = 0;
    
    // Очищуємо старі файли якщо перевищуємо квоту
    enforceQuota();
}

void LogStorage::enforceQuota() {
    size_t usedSpace = getUsedSpace();
    
    if (usedSpace <= m_config.maxTotalSize) {
        return;
    }
    
    ESP_LOGW(TAG, "Enforcing quota: %zu / %zu bytes", usedSpace, m_config.maxTotalSize);
    
    // Видаляємо найстаріші архівні файли
    for (int i = m_config.maxArchiveFiles; i >= 1; i--) {
        std::string filepath = getArchiveLogPath(i);
        if (deleteFile(filepath)) {
            usedSpace = getUsedSpace();
            if (usedSpace <= m_config.maxTotalSize) {
                break;
            }
        }
    }
}

size_t LogStorage::getFileSize(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

bool LogStorage::deleteFile(const std::string& path) {
    if (unlink(path.c_str()) == 0) {
        ESP_LOGD(TAG, "Deleted file: %s", path.c_str());
        return true;
    }
    return false;
}

bool LogStorage::renameFile(const std::string& from, const std::string& to) {
    if (rename(from.c_str(), to.c_str()) == 0) {
        ESP_LOGD(TAG, "Renamed: %s -> %s", from.c_str(), to.c_str());
        return true;
    }
    return false;
}

FILE* LogStorage::openLogFile(const std::string& path, const char* mode) {
    FILE* file = fopen(path.c_str(), mode);
    if (!file) {
        ESP_LOGD(TAG, "Failed to open file: %s", path.c_str());
    }
    return file;
}

void LogStorage::closeLogFile(FILE* file) {
    if (file) {
        fclose(file);
    }
}

} // namespace ModESP
