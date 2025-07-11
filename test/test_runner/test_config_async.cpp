/**
 * @file test_config_async.cpp
 * @brief Unit tests for Async ConfigManager
 */

#include "unity.h"
#include "config_manager.h"
#include "config_manager_async.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <string>
#include <inttypes.h>

static const char* TAG = "TestConfigAsync";

// Helper to generate large JSON
static nlohmann::json generate_large_json(size_t target_size) {
    nlohmann::json data;
    std::string large_string(target_size / 10, 'X');
    
    for (int i = 0; i < 10; i++) {
        data["field_" + std::to_string(i)] = large_string;
    }
    
    return data;
}

// Test cases
void test_config_async_init() {
    ConfigManagerAsync::AsyncConfig config;
    config.write_queue_size = 5;
    config.watchdog_feed_interval = 20;    
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManagerAsync::init_async(config));
    
    // Test double init
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManagerAsync::init_async(config));
}

void test_config_async_schedule_save() {
    // Initialize
    ConfigManager::init();
    ConfigManagerAsync::init_async();
    
    // Set some data
    ConfigManager::set("test.async.value", 42);
    ConfigManager::set("test.async.string", "test data");
    
    // Schedule save
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManagerAsync::schedule_save("test"));
    
    // Wait for processing
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // Check stats
    auto stats = ConfigManagerAsync::get_async_stats();
    TEST_ASSERT_GREATER_THAN(0, stats.completed_saves);
}

void test_config_async_batch_saves() {
    // Schedule multiple saves quickly
    for (int i = 0; i < 5; i++) {
        std::string module = "module" + std::to_string(i);
        ConfigManager::set(module + ".data", i * 100);
        ConfigManagerAsync::schedule_save(module);
    }
    
    // Should batch these together
    vTaskDelay(pdMS_TO_TICKS(300));
    
    auto stats = ConfigManagerAsync::get_async_stats();
    ESP_LOGI(TAG, "Batched %d saves in %" PRIu32 " operations", 
             5, stats.completed_saves);
    
    // Should be less than 5 due to batching
    TEST_ASSERT_LESS_OR_EQUAL(3, stats.completed_saves);
}

void test_config_async_watchdog_handling() {
    // Create very large data
    nlohmann::json huge_data = generate_large_json(50000); // 50KB
    
    ConfigManager::set("huge.module.data", huge_data);
    
    // This should not trigger watchdog
    uint64_t start = esp_timer_get_time();
    ConfigManagerAsync::schedule_save("huge");
    ConfigManagerAsync::flush_pending_saves(5000);
    uint64_t duration = esp_timer_get_time() - start;    
    ESP_LOGI(TAG, "Large save completed in %lld ms", duration / 1000);
    TEST_ASSERT_LESS_THAN(10000000, duration); // < 10 seconds
    
    // Verify save completed
    auto stats = ConfigManagerAsync::get_async_stats();
    TEST_ASSERT_EQUAL(0, stats.pending_saves);
}

void test_config_async_flush() {
    // Schedule multiple saves
    for (int i = 0; i < 10; i++) {
        ConfigManager::set("flush.test." + std::to_string(i), i);
        ConfigManagerAsync::schedule_save_all();
    }
    
    // Flush with timeout
    esp_err_t ret = ConfigManagerAsync::flush_pending_saves(3000);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // All saves should be complete
    auto stats = ConfigManagerAsync::get_async_stats();
    TEST_ASSERT_EQUAL(0, stats.pending_saves);
}

void test_config_async_stop() {
    // Stop and cleanup
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManagerAsync::stop_async());
    
    // Should handle double stop
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManagerAsync::stop_async());
}

// Test group runner
void test_config_async_group(void) {
    // Add task to watchdog
    esp_task_wdt_add(NULL);
    
    RUN_TEST(test_config_async_init);
    RUN_TEST(test_config_async_schedule_save);
    RUN_TEST(test_config_async_batch_saves);
    RUN_TEST(test_config_async_watchdog_handling);
    RUN_TEST(test_config_async_flush);
    RUN_TEST(test_config_async_stop);
    
    // Remove from watchdog
    esp_task_wdt_delete(NULL);
}
