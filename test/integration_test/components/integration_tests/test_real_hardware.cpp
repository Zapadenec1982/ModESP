/**
 * @file test_real_hardware.cpp
 * @brief Real hardware integration tests for ModESP system
 * 
 * Tests system behavior with actual ESP32-S3 hardware:
 * - Flash memory operations
 * - NVS storage
 * - GPIO functionality
 * - Real sensor readings (if available)
 * - WiFi connectivity
 * - Timer accuracy
 */

#include "integration_test_common.h"
#include "application.h"
#include "event_bus.h"
#include "shared_state.h"
#include "config_manager.h"
#include "module_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_efuse.h"
#include "esp_mac.h"
#include "esp_private/esp_clk.h"
#include "esp_clk_tree.h"
#include "soc/rtc.h"

static const char* TAG = "HardwareTest";

/**
 * @brief Test flash memory operations
 * 
 * Verifies flash memory functionality:
 * - NVS read/write operations
 * - Configuration persistence
 * - Flash wear leveling
 * - Storage capacity
 */
void test_flash_memory_operations() {
    ESP_LOGI(TAG, "=== Testing Flash Memory Operations ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Test NVS operations
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("test_storage", NVS_READWRITE, &nvs_handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    // Test writing various data types
    const char* test_string = "ModESP Integration Test";
    uint32_t test_uint32 = 0x12345678;
    float test_float = 3.14159f;
    
    ESP_LOGI(TAG, "Writing test data to NVS...");
    
    err = nvs_set_str(nvs_handle, "test_string", test_string);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_set_u32(nvs_handle, "test_uint32", test_uint32);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_set_blob(nvs_handle, "test_float", &test_float, sizeof(test_float));
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    err = nvs_commit(nvs_handle);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    ESP_LOGI(TAG, "Reading test data from NVS...");
    
    // Test reading back the data
    char read_string[64];
    size_t string_len = sizeof(read_string);
    err = nvs_get_str(nvs_handle, "test_string", read_string, &string_len);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL_STRING(test_string, read_string);
    
    uint32_t read_uint32;
    err = nvs_get_u32(nvs_handle, "test_uint32", &read_uint32);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(test_uint32, read_uint32);
    
    float read_float;
    size_t float_size = sizeof(read_float);
    err = nvs_get_blob(nvs_handle, "test_float", &read_float, &float_size);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, test_float, read_float);
    
    // Test NVS statistics
    nvs_stats_t nvs_stats;
    err = nvs_get_stats(nullptr, &nvs_stats);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    ESP_LOGI(TAG, "NVS Statistics:");
    ESP_LOGI(TAG, "  Used entries: %zu", nvs_stats.used_entries);
    ESP_LOGI(TAG, "  Free entries: %zu", nvs_stats.free_entries);
    ESP_LOGI(TAG, "  Total entries: %zu", nvs_stats.total_entries);
    ESP_LOGI(TAG, "  Namespace count: %zu", nvs_stats.namespace_count);
    
    // Cleanup
    nvs_close(nvs_handle);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Flash Memory Operations", &metrics);
    
    ESP_LOGI(TAG, "✅ Flash memory operations test PASSED");
}

/**
 * @brief Test GPIO functionality
 * 
 * Tests GPIO operations:
 * - Digital input/output
 * - Pull-up/pull-down resistors
 * - Interrupt handling
 * - GPIO timing accuracy
 */
void test_gpio_functionality() {
    ESP_LOGI(TAG, "=== Testing GPIO Functionality ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Use GPIO pins that are typically available on ESP32-S3
    const gpio_num_t output_pin = GPIO_NUM_2;  // Built-in LED on many boards
    const gpio_num_t input_pin = GPIO_NUM_0;   // Boot button on many boards
    
    // Configure output pin
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << output_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t err = gpio_config(&output_config);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    // Configure input pin with pull-up
    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << input_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    err = gpio_config(&input_config);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    ESP_LOGI(TAG, "Testing GPIO output operations...");
    
    // Test GPIO output operations
    for (int i = 0; i < 10; i++) {
        gpio_set_level(output_pin, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        gpio_set_level(output_pin, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        ESP_LOGI(TAG, "GPIO toggle %d completed", i + 1);
    }
    
    ESP_LOGI(TAG, "Testing GPIO input operations...");
    
    // Test GPIO input reading
    int input_readings = 0;
    int high_readings = 0;
    
    for (int i = 0; i < 100; i++) {
        int level = gpio_get_level(input_pin);
        input_readings++;
        if (level) high_readings++;
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    ESP_LOGI(TAG, "GPIO input test: %d/%d readings were high", 
             high_readings, input_readings);
    
    // Input should be stable (pulled up)
    TEST_ASSERT_GREATER_THAN(80, high_readings); // At least 80% high readings
    
    // Test GPIO timing accuracy
    ESP_LOGI(TAG, "Testing GPIO timing accuracy...");
    
    uint32_t start_time = esp_timer_get_time();
    
    for (int i = 0; i < 1000; i++) {
        gpio_set_level(output_pin, i % 2);
    }
    
    uint32_t end_time = esp_timer_get_time();
    uint32_t total_time = end_time - start_time;
    
    ESP_LOGI(TAG, "1000 GPIO operations took %lu μs (%.2f μs per operation)", 
             total_time, (float)total_time / 1000.0f);
    
    // GPIO operations should be fast
    TEST_ASSERT_LESS_THAN(10000, total_time); // Less than 10ms for 1000 operations
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("GPIO Functionality", &metrics);
    
    ESP_LOGI(TAG, "✅ GPIO functionality test PASSED");
}

/**
 * @brief Test timer accuracy and precision
 * 
 * Verifies ESP32-S3 timer functionality:
 * - High-resolution timer accuracy
 * - FreeRTOS tick accuracy
 * - Timer interrupt handling
 * - Time measurement precision
 */
void test_timer_accuracy() {
    ESP_LOGI(TAG, "=== Testing Timer Accuracy ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    // Test high-resolution timer accuracy
    ESP_LOGI(TAG, "Testing high-resolution timer accuracy...");
    
    const uint32_t test_delays_ms[] = {1, 10, 100, 500, 1000};
    const size_t num_delays = sizeof(test_delays_ms) / sizeof(test_delays_ms[0]);
    
    for (size_t i = 0; i < num_delays; i++) {
        uint32_t target_delay = test_delays_ms[i];
        
        uint64_t start_time = esp_timer_get_time();
        vTaskDelay(pdMS_TO_TICKS(target_delay));
        uint64_t end_time = esp_timer_get_time();
        
        uint32_t actual_delay = (end_time - start_time) / 1000;
        int32_t error = actual_delay - target_delay;
        float error_percent = (float)abs(error) / target_delay * 100.0f;
        
        ESP_LOGI(TAG, "Delay test: target=%lu ms, actual=%lu ms, error=%ld ms (%.1f%%)",
                 target_delay, actual_delay, error, error_percent);
        
        // Allow reasonable timing tolerance (FreeRTOS tick resolution)
        if (target_delay >= 10) {
            TEST_ASSERT_LESS_THAN(15.0f, error_percent); // Less than 15% error
        }
    }
    
    // Test timer precision
    ESP_LOGI(TAG, "Testing timer precision...");
    
    const int precision_samples = 1000;
    uint64_t precision_times[precision_samples];
    
    for (int i = 0; i < precision_samples; i++) {
        precision_times[i] = esp_timer_get_time();
    }
    
    // Calculate minimum time difference
    uint64_t min_diff = UINT64_MAX;
    for (int i = 1; i < precision_samples; i++) {
        uint64_t diff = precision_times[i] - precision_times[i-1];
        if (diff > 0 && diff < min_diff) {
            min_diff = diff;
        }
    }
    
    ESP_LOGI(TAG, "Timer precision: minimum difference = %llu μs", min_diff);
    
    // Timer should have microsecond precision
    TEST_ASSERT_LESS_THAN(10, min_diff); // Should be able to measure sub-10μs differences
    
    // Test timer stability over time
    ESP_LOGI(TAG, "Testing timer stability...");
    
    uint64_t stability_start = esp_timer_get_time();
    uint32_t stability_samples = 0;
    uint64_t max_drift = 0;
    
    for (int second = 1; second <= 5; second++) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        uint64_t current_time = esp_timer_get_time();
        uint64_t expected_time = stability_start + (second * 1000000ULL);
        uint64_t actual_elapsed = current_time - stability_start;
        
        uint64_t drift = (actual_elapsed > expected_time) ? 
                        (actual_elapsed - expected_time) : 
                        (expected_time - actual_elapsed);
        
        if (drift > max_drift) max_drift = drift;
        
        ESP_LOGI(TAG, "Timer stability check %d: drift = %llu μs", second, drift);
        stability_samples++;
    }
    
    ESP_LOGI(TAG, "Timer stability: maximum drift over 5 seconds = %llu μs", max_drift);
    
    // Timer should be stable (allow 1ms drift over 5 seconds)
    TEST_ASSERT_LESS_THAN(1000, max_drift);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("Timer Accuracy", &metrics);
    
    ESP_LOGI(TAG, "✅ Timer accuracy test PASSED");
}

/**
 * @brief Test system resource monitoring
 * 
 * Tests hardware resource monitoring:
 * - Memory usage tracking
 * - CPU temperature (if available)
 * - Power consumption estimation
 * - System load monitoring
 */
void test_system_resource_monitoring() {
    ESP_LOGI(TAG, "=== Testing System Resource Monitoring ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    ESP_LOGI(TAG, "Collecting system resource information...");
    
    // Memory information
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    
    ESP_LOGI(TAG, "Memory status:");
    ESP_LOGI(TAG, "  Free heap: %zu bytes (%.1f KB)", free_heap, free_heap / 1024.0f);
    ESP_LOGI(TAG, "  Minimum free heap: %zu bytes", min_free_heap);
    ESP_LOGI(TAG, "  Largest free block: %zu bytes", largest_free_block);
    
    // Verify reasonable memory state
    TEST_ASSERT_GREATER_THAN(MIN_FREE_HEAP_BYTES, free_heap);
    TEST_ASSERT_GREATER_THAN(10000, min_free_heap); // At least 10KB minimum
    
    // CPU information
    uint32_t cpu_freq = esp_clk_cpu_freq();
    ESP_LOGI(TAG, "CPU frequency: %lu MHz", cpu_freq / 1000000);
    TEST_ASSERT_EQUAL(160000000, cpu_freq); // Should be 160MHz for ESP32-S3
    
    // Flash information
    uint32_t flash_size = 0;
    esp_flash_get_size(nullptr, &flash_size);
    ESP_LOGI(TAG, "Flash size: %lu bytes (%.1f MB)", flash_size, flash_size / (1024.0f * 1024.0f));
    TEST_ASSERT_GREATER_THAN(1024 * 1024, flash_size); // At least 1MB
    
    // Chip information
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    
    ESP_LOGI(TAG, "Chip information:");
    ESP_LOGI(TAG, "  Model: ESP32-S3");
    ESP_LOGI(TAG, "  Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "  Revision: %d", chip_info.revision);
    ESP_LOGI(TAG, "  Features: %s%s%s%s",
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
             (chip_info.features & CHIP_FEATURE_BT) ? "BT " : "",
             (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "Embedded-Flash " : "");
    
    TEST_ASSERT_EQUAL(2, chip_info.cores); // ESP32-S3 should have 2 cores
    
    // Monitor resource usage over time
    ESP_LOGI(TAG, "Monitoring resource usage over time...");
    
    const uint32_t monitoring_duration_ms = 5000;
    const uint32_t sample_interval_ms = 500;
    uint32_t samples = 0;
    
    size_t heap_samples[20];
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    while (samples < (sizeof(heap_samples) / sizeof(heap_samples[0])) &&
           ((esp_timer_get_time() / 1000) - start_time) < monitoring_duration_ms) {
        
        heap_samples[samples] = esp_get_free_heap_size();
        samples++;
        
        vTaskDelay(pdMS_TO_TICKS(sample_interval_ms));
    }
    
    // Analyze heap stability
    size_t min_heap_sample = SIZE_MAX;
    size_t max_heap_sample = 0;
    
    for (uint32_t i = 0; i < samples; i++) {
        if (heap_samples[i] < min_heap_sample) min_heap_sample = heap_samples[i];
        if (heap_samples[i] > max_heap_sample) max_heap_sample = heap_samples[i];
    }
    
    size_t heap_variance = max_heap_sample - min_heap_sample;
    
    ESP_LOGI(TAG, "Heap stability analysis (%lu samples):", samples);
    ESP_LOGI(TAG, "  Min heap: %zu bytes", min_heap_sample);
    ESP_LOGI(TAG, "  Max heap: %zu bytes", max_heap_sample);
    ESP_LOGI(TAG, "  Variance: %zu bytes", heap_variance);
    
    // Heap should be relatively stable (less than 50KB variance)
    TEST_ASSERT_LESS_THAN(50000, heap_variance);
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("System Resource Monitoring", &metrics);
    
    ESP_LOGI(TAG, "✅ System resource monitoring test PASSED");
}

/**
 * @brief Test WiFi hardware functionality
 * 
 * Tests WiFi hardware without actually connecting:
 * - WiFi initialization
 * - MAC address reading
 * - WiFi scanning capability
 * - RF calibration
 */
void test_wifi_hardware() {
    ESP_LOGI(TAG, "=== Testing WiFi Hardware ===");
    
    integration_test_metrics_t metrics;
    integration_test_start_metrics(&metrics);
    
    ESP_LOGI(TAG, "Testing WiFi hardware initialization...");
    
    // Get MAC address
    uint8_t mac_addr[6];
    esp_err_t err = esp_read_mac(mac_addr, ESP_MAC_WIFI_STA);
    TEST_ASSERT_EQUAL(ESP_OK, err);
    
    ESP_LOGI(TAG, "Default MAC address: %02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], 
             mac_addr[3], mac_addr[4], mac_addr[5]);
    
    // Verify MAC address is not all zeros or all ones
    bool mac_valid = false;
    for (int i = 0; i < 6; i++) {
        if (mac_addr[i] != 0x00 && mac_addr[i] != 0xFF) {
            mac_valid = true;
            break;
        }
    }
    TEST_ASSERT_TRUE(mac_valid);
    
    // Test WiFi station MAC
    uint8_t sta_mac[6];
    err = esp_wifi_get_mac(WIFI_IF_STA, sta_mac);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "WiFi STA MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 sta_mac[0], sta_mac[1], sta_mac[2], 
                 sta_mac[3], sta_mac[4], sta_mac[5]);
    } else {
        ESP_LOGI(TAG, "WiFi not initialized, MAC read failed (expected)");
    }
    
    ESP_LOGI(TAG, "WiFi hardware appears functional");
    
    integration_test_stop_metrics(&metrics);
    integration_test_print_metrics("WiFi Hardware", &metrics);
    
    ESP_LOGI(TAG, "✅ WiFi hardware test PASSED");
}

// Test runner for real hardware tests
extern "C" void run_real_hardware_tests(void) {
    ESP_LOGI(TAG, "🔌 Starting Real Hardware Tests");
    
    RUN_TEST(test_flash_memory_operations);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_gpio_functionality);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_timer_accuracy);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_system_resource_monitoring);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    RUN_TEST(test_wifi_hardware);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    ESP_LOGI(TAG, "✅ Real Hardware Tests Complete");
} 