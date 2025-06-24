#include <gtest/gtest.h>
#include "logger/logger_module.h"
#include "logger/include/logger_interface.h"
#include "core/shared_state.h"
#include "core/config_manager.h"

using namespace ModESP;

class LoggerModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ініціалізуємо глобальні залежності  
        SharedState::createInstance();
        
        // Створюємо LoggerModule
        logger = std::make_unique<LoggerModule>();
    }
    
    void TearDown() override {
        logger.reset();
        SharedState::destroyInstance();
    }
    
    std::unique_ptr<LoggerModule> logger;
};

// Тест базової функціональності
TEST_F(LoggerModuleTest, BasicLogging) {
    // Ініціалізуємо модуль
    EXPECT_EQ(ESP_OK, logger->init());
    EXPECT_EQ(ESP_OK, logger->start());
    
    // Тестуємо базове логування
    logger->log(LogLevel::INFO, "TEST", "Test message %d", 123);
    
    // Перевіряємо що лог записався
    EXPECT_GT(logger->getLogCount(), 0);
    
    // Зупиняємо модуль
    EXPECT_EQ(ESP_OK, logger->stop());
}

// Тест структурованих подій
TEST_F(LoggerModuleTest, EventLogging) {
    EXPECT_EQ(ESP_OK, logger->init());
    EXPECT_EQ(ESP_OK, logger->start());
    
    // Логуємо температурну подію
    logger->logEvent(EventCode::TEMP_ALARM_HIGH, 2500, "High temp");
    
    // Логуємо дані сенсора
    logger->logSensorData(1, 25.5f);
    
    // Логуємо цикл компресора
    logger->logCompressorCycle(true, 3600);
    
    // Перевіряємо кількість записів
    EXPECT_GE(logger->getLogCount(), 3);
    
    EXPECT_EQ(ESP_OK, logger->stop());
}

// Тест фільтрації логів
TEST_F(LoggerModuleTest, LogFiltering) {
    EXPECT_EQ(ESP_OK, logger->init());
    EXPECT_EQ(ESP_OK, logger->start());
    
    // Додаємо логи різних рівнів
    logger->log(LogLevel::DEBUG, "TEST", "Debug message");
    logger->log(LogLevel::INFO, "TEST", "Info message");
    logger->log(LogLevel::WARNING, "TEST", "Warning message");
    logger->log(LogLevel::ERROR, "TEST", "Error message");
    
    // Фільтруємо тільки помилки та вище
    LogFilter filter;
    filter.minLevel = LogLevel::ERROR;
    filter.maxEntries = 10;
    
    auto logs = logger->readLogs(filter);
    
    // Перевіряємо що отримали тільки помилки
    for (const auto& entry : logs) {
        EXPECT_GE(static_cast<LogLevel>(entry.level), LogLevel::ERROR);
    }
    
    EXPECT_EQ(ESP_OK, logger->stop());
}

// Тест експорту логів
TEST_F(LoggerModuleTest, LogExport) {
    EXPECT_EQ(ESP_OK, logger->init());
    EXPECT_EQ(ESP_OK, logger->start());
    
    // Додаємо кілька записів
    logger->log(LogLevel::INFO, "TEST", "Export test 1");
    logger->log(LogLevel::WARNING, "TEST", "Export test 2");
    
    // Тестуємо CSV експорт
    std::string csvOutput;
    EXPECT_TRUE(logger->exportLogs("csv", csvOutput));
    EXPECT_FALSE(csvOutput.empty());
    EXPECT_NE(csvOutput.find("Timestamp"), std::string::npos);
    
    // Тестуємо JSON експорт
    std::string jsonOutput;
    EXPECT_TRUE(logger->exportLogs("json", jsonOutput));
    EXPECT_FALSE(jsonOutput.empty());
    EXPECT_NE(jsonOutput.find("logs"), std::string::npos);
    
    EXPECT_EQ(ESP_OK, logger->stop());
}

// Тест статистики
TEST_F(LoggerModuleTest, Statistics) {
    EXPECT_EQ(ESP_OK, logger->init());
    EXPECT_EQ(ESP_OK, logger->start());
    
    // Додаємо логи
    logger->log(LogLevel::INFO, "TEST", "Stats test");
    logger->logEvent(EventCode::TEMP_ALARM_HIGH, 2500);
    
    // Перевіряємо статистику
    std::string stats;
    logger->getStatistics(stats);
    
    EXPECT_FALSE(stats.empty());
    EXPECT_NE(stats.find("Total logs"), std::string::npos);
    EXPECT_NE(stats.find("Used space"), std::string::npos);
    
    // Перевіряємо використання пам'яті
    EXPECT_GT(logger->getUsedSpace(), 0);
    EXPECT_EQ(logger->getTotalSpace(), 256 * 1024); // 256KB
    
    EXPECT_EQ(ESP_OK, logger->stop());
}

// Тест HACCP подій
TEST_F(LoggerModuleTest, HACCPLogging) {
    EXPECT_EQ(ESP_OK, logger->init());
    EXPECT_EQ(ESP_OK, logger->start());
    
    // Логуємо HACCP порушення
    logger->logHACCPEvent(EventCode::HACCP_TEMP_VIOLATION, 8.5f);
    
    // HACCP події повинні бути критичними
    LogFilter filter;
    filter.minLevel = LogLevel::CRITICAL;
    auto logs = logger->readLogs(filter);
    
    EXPECT_GE(logs.size(), 1);
    
    EXPECT_EQ(ESP_OK, logger->stop());
}
