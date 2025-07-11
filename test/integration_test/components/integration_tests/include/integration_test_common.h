/**
 * @file integration_test_common.h
 * @brief Common utilities and definitions for integration tests
 */

#pragma once

#include "unity.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "application.h"

#ifdef __cplusplus
extern "C" {
#endif

// Test configuration
#define INTEGRATION_TEST_TIMEOUT_MS     30000   // 30 seconds max per test
#define SYSTEM_STABILIZE_TIME_MS        2000    // Time for system to stabilize
#define STRESS_TEST_DURATION_MS         10000   // 10 seconds stress test
#define HEALTH_CHECK_INTERVAL_MS        1000    // Health check every second

// Memory thresholds
#define MIN_FREE_HEAP_BYTES            100000   // Minimum free heap
#define CRITICAL_FREE_HEAP_BYTES       50000    // Critical heap level
#define MAX_HEAP_FRAGMENTATION_PERCENT 20       // Max fragmentation allowed

// Performance thresholds
#define MAX_CYCLE_TIME_US              10000    // Max main loop cycle time
#define MAX_CPU_USAGE_PERCENT          80       // Max CPU usage
#define MIN_STACK_FREE_BYTES           1024     // Min stack free space

// Test utilities
#define INTEGRATION_TEST_ASSERT_MEMORY_OK() \
    do { \
        size_t free_heap = esp_get_free_heap_size(); \
        TEST_ASSERT_GREATER_THAN(MIN_FREE_HEAP_BYTES, free_heap); \
        ESP_LOGI("MemCheck", "Free heap: %zu bytes", free_heap); \
    } while(0)

#define INTEGRATION_TEST_ASSERT_SYSTEM_HEALTHY() \
    do { \
        TEST_ASSERT_TRUE(Application::check_health()); \
        TEST_ASSERT_TRUE(Application::is_running()); \
        INTEGRATION_TEST_ASSERT_MEMORY_OK(); \
    } while(0)

#define INTEGRATION_TEST_WAIT_STABLE(time_ms) \
    do { \
        ESP_LOGI("TestWait", "Waiting %d ms for system to stabilize", time_ms); \
        vTaskDelay(pdMS_TO_TICKS(time_ms)); \
    } while(0)

// Test result tracking
typedef struct {
    uint32_t start_time_ms;
    uint32_t end_time_ms;
    size_t start_free_heap;
    size_t end_free_heap;
    size_t min_free_heap;
    uint32_t max_cycle_time_us;
    uint8_t max_cpu_usage;
    bool system_stable;
} integration_test_metrics_t;

/**
 * @brief Start integration test metrics collection
 * @param metrics Pointer to metrics structure
 */
void integration_test_start_metrics(integration_test_metrics_t* metrics);

/**
 * @brief Stop integration test metrics collection
 * @param metrics Pointer to metrics structure
 */
void integration_test_stop_metrics(integration_test_metrics_t* metrics);

/**
 * @brief Print integration test metrics
 * @param test_name Name of the test
 * @param metrics Pointer to metrics structure
 */
void integration_test_print_metrics(const char* test_name, const integration_test_metrics_t* metrics);

/**
 * @brief Verify system stability during test
 * @param duration_ms How long to monitor
 * @return true if system remained stable
 */
bool integration_test_verify_stability(uint32_t duration_ms);

/**
 * @brief Generate system load for stress testing
 * @param duration_ms How long to generate load
 * @param intensity Load intensity (1-10)
 */
void integration_test_generate_load(uint32_t duration_ms, uint8_t intensity);

/**
 * @brief Wait for application to reach running state
 * @param timeout_ms Maximum time to wait
 * @return true if application is running
 */
bool integration_test_wait_for_running(uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif 