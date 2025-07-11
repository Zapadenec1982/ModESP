/**
 * @file test_logger.cpp
 * @brief Unit tests for LoggerModule
 */

#include "unity.h"
#include "logger_module.h"
#include "logger_interface.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <string>
#include <vector>

using namespace ModESP;

static const char* TAG = "TestLogger";
static LoggerModule* test_logger = nullptr;

// Test fixtures
static void reset_test_state() {
    ESP_LOGI(TAG, "Resetting logger test state...");
    
    if (test_logger) {
        test_logger->stop();
        delete test_logger;
        test_logger = nullptr;
    }
    
    // Small delay to ensure cleanup
    vTaskDelay(pdMS_TO_TICKS(100));
}

static void setup_test_logger() {
    test_logger = new LoggerModule();
    
    // Configure for testing
    nlohmann::json config = {
        {"enabled", true},
        {"defaultLevel", static_cast<int>(LogLevel::DEBUG)},
        {"maxFileSize", 32 * 1024},
        {"maxTotalSize", 128 * 1024},
        {"maxArchiveFiles", 2},
        {"flushInterval", 1000},
        {"haccp", true}
    };
    
    test_logger->configure(config);
    esp_err_t result = test_logger->init();
    TEST_ASSERT_EQUAL(ESP_OK, result);
    
    vTaskDelay(pdMS_TO_TICKS(200));
}

// Basic functionality tests
void test_logger_init() {
    reset_test_state();
    
    LoggerModule logger;
    esp_err_t result = logger.init();
    TEST_ASSERT_EQUAL(ESP_OK, result);
    
    logger.stop();
    ESP_LOGI(TAG, "Logger init test passed");
}

void test_logger_basic_logging() {
    reset_test_state();
    setup_test_logger();
    
    // Test different log levels
    test_logger->logFormatted(LogLevel::DEBUG, "TestModule", "Debug message");
    test_logger->logFormatted(LogLevel::INFO, "TestModule", "Info message");
    test_logger->logFormatted(LogLevel::WARNING, "TestModule", "Warning message");
    test_logger->logFormatted(LogLevel::ERROR, "TestModule", "Error message");
    test_logger->logFormatted(LogLevel::CRITICAL, "TestModule", "Critical message");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    size_t log_count = test_logger->getLogCount();
    TEST_ASSERT_GREATER_THAN(0, log_count);
    
    ESP_LOGI(TAG, "Basic logging test passed, %zu logs created", log_count);
}

void test_logger_event_logging() {
    reset_test_state();
    setup_test_logger();
    
    // Test different event types
    test_logger->logEvent(EventCode::SYSTEM_START, 0, "System starting");
    test_logger->logEvent(EventCode::TEMP_ALARM_HIGH, 250, "High temp alarm");
    test_logger->logEvent(EventCode::COMPRESSOR_ON, 1, "Compressor started");
    test_logger->logEvent(EventCode::DEFROST_START, 0, "Defrost cycle");
    test_logger->logEvent(EventCode::HACCP_TEMP_VIOLATION, -50, "HACCP violation");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    size_t log_count = test_logger->getLogCount();
    TEST_ASSERT_GREATER_OR_EQUAL(5, log_count);
    
    ESP_LOGI(TAG, "Event logging test passed, %zu logs created", log_count);
}

void test_logger_specialized_logging() {
    reset_test_state();
    setup_test_logger();
    
    // Test specialized logging methods
    test_logger->logSensorData(1, 23.5f);
    test_logger->logSensorData(2, -18.2f);
    test_logger->logCompressorCycle(true, 120000);
    test_logger->logDefrostCycle(1, 300);
    test_logger->logHACCPEvent(EventCode::HACCP_TEMP_VIOLATION, 15.0f);
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    size_t log_count = test_logger->getLogCount();
    TEST_ASSERT_GREATER_OR_EQUAL(5, log_count);
    
    ESP_LOGI(TAG, "Specialized logging test passed, %zu logs created", log_count);
}

void test_logger_log_reading() {
    reset_test_state();
    setup_test_logger();
    
    // Create some test logs
    test_logger->logFormatted(LogLevel::INFO, "Reader", "Test message 1");
    test_logger->logFormatted(LogLevel::WARNING, "Reader", "Test message 2");
    test_logger->logFormatted(LogLevel::ERROR, "Reader", "Test message 3");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Test reading with filters
    LogFilter filter_all;
    filter_all.minLevel = LogLevel::DEBUG;
    filter_all.maxEntries = 100;
    
    std::vector<LogEntry> logs = test_logger->readLogs(filter_all);
    TEST_ASSERT_GREATER_OR_EQUAL(3, logs.size());
    
    ESP_LOGI(TAG, "Log reading test passed, read %zu logs", logs.size());
}

void test_logger_statistics() {
    reset_test_state();
    setup_test_logger();
    
    // Generate various logs
    test_logger->logFormatted(LogLevel::DEBUG, "Stats", "Debug message");
    test_logger->logFormatted(LogLevel::INFO, "Stats", "Info message");
    test_logger->logFormatted(LogLevel::WARNING, "Stats", "Warning message");
    test_logger->logFormatted(LogLevel::ERROR, "Stats", "Error message");
    test_logger->logEvent(EventCode::SYSTEM_START, 0, "Event log");
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    std::string stats;
    test_logger->getStatistics(stats);
    
    TEST_ASSERT_FALSE(stats.empty());
    
    ESP_LOGI(TAG, "Statistics test passed");
    ESP_LOGI(TAG, "Logger statistics:\n%s", stats.c_str());
}

// Test group runner
void test_logger_group(void) {
    ESP_LOGI(TAG, "=== Starting Logger Tests ===");
    
    RUN_TEST(test_logger_init);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    RUN_TEST(test_logger_basic_logging);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    RUN_TEST(test_logger_event_logging);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    RUN_TEST(test_logger_specialized_logging);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    RUN_TEST(test_logger_log_reading);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    RUN_TEST(test_logger_statistics);
    
    // Final cleanup
    reset_test_state();
    
    ESP_LOGI(TAG, "=== Logger Tests Complete ===");
} 