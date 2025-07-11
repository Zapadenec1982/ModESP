/**
 * @file test_config_manager.cpp
 * @brief Unit tests for ConfigManager component
 * 
 * These tests reflect the real production usage pattern:
 * - Initialize ONCE at startup (like Application::init())
 * - Load configuration ONCE 
 * - Use get/set/save operations throughout runtime
 * - Never re-initialize
 */

#include "unity.h"
#include "config_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <nlohmann/json.hpp>
#include <esp_littlefs.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

static const char* TAG = "TestConfigManager";

// Helper to recursively remove directory
static void remove_directory(const char* path) {
    DIR* dir = opendir(path);
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            
            struct stat st;
            if (stat(full_path, &st) == 0) {
                if (S_ISDIR(st.st_mode)) {
                    remove_directory(full_path);  // Recursive call
                } else {
                    unlink(full_path);  // Remove file
                }
            }
        }
        closedir(dir);
        rmdir(path);  // Remove empty directory
    }
}

// Helper to clear LittleFS for clean tests
static void clear_test_storage() {
    // Remove config files specifically
    const char* config_files[] = {
        "/storage/configs/system.json",
        "/storage/configs/climate.json", 
        "/storage/configs/sensors.json",
        "/storage/configs/actuators.json",
        "/storage/configs/alarms.json",
        "/storage/configs/network.json",
        "/storage/configs/ui.json",
        "/storage/configs/logging.json",
        "/storage/configs/wifi.json",
        "/storage/configs/rtc.json",
        "/storage/configs/version.dat"
    };
    
    for (size_t i = 0; i < sizeof(config_files) / sizeof(config_files[0]); i++) {
        unlink(config_files[i]);  // Ignore errors - file might not exist
    }
    
    ESP_LOGI(TAG, "Cleared test storage data");
}

// Test basic initialization (production pattern)
void test_config_manager_production_startup() {
    ESP_LOGI(TAG, "Testing production startup pattern...");
    
    // Clear any existing config like on first boot
    clear_test_storage();
    
    // Mimic Application::init() sequence
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::init());  // Initialize once
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::load());  // Load once
    
    // ✅ CORRECTED: On first boot with embedded defaults, system should be dirty
    // because embedded configs need to be saved to LittleFS for persistence
    TEST_ASSERT_TRUE(ConfigManager::is_dirty());  // ✅ Correct expectation!
    
    // Verify basic functionality works
    nlohmann::json value = ConfigManager::get("system.device_name");
    TEST_ASSERT_FALSE(value.is_null());
    
    // Save the embedded defaults to LittleFS (like real system would do)
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::save());
    TEST_ASSERT_FALSE(ConfigManager::is_dirty());  // Now it should be clean
    
    ESP_LOGI(TAG, "Production startup sequence: PASS");
}

// Test get/set operations (main runtime operations)
void test_config_manager_runtime_operations() {
    ESP_LOGI(TAG, "Testing runtime get/set operations...");
    
    // Test getting existing value
    nlohmann::json system_config = ConfigManager::get("system");
    TEST_ASSERT_FALSE(system_config.is_null());
    
    // Test setting new value (typical UI operation)
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::set("test.value", 42));
    TEST_ASSERT_TRUE(ConfigManager::is_dirty());
    
    // Verify the value was set
    nlohmann::json test_value = ConfigManager::get("test.value");
    TEST_ASSERT_EQUAL(42, test_value.get<int>());
    
    // Test save operation (typical UI save action)
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::save());
    TEST_ASSERT_FALSE(ConfigManager::is_dirty());
    
    ESP_LOGI(TAG, "Runtime operations: PASS");
}

// Test configuration persistence (load after save)
void test_config_manager_persistence() {
    ESP_LOGI(TAG, "Testing configuration persistence...");
    
    // Set a test value and save
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::set("persist.test", "saved_value"));
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::save());
    
    // Now load and verify it persisted (simulating restart)
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::load());
    
    nlohmann::json loaded_value = ConfigManager::get("persist.test");
    TEST_ASSERT_FALSE(loaded_value.is_null());
    TEST_ASSERT_EQUAL_STRING("saved_value", loaded_value.get<std::string>().c_str());
    
    ESP_LOGI(TAG, "Persistence: PASS");
}

// Test discard changes (typical UI "Cancel" operation)
void test_config_manager_discard_changes() {
    ESP_LOGI(TAG, "Testing discard changes...");
    
    // Make some changes
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::set("temp.change1", "value1"));
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::set("temp.change2", 123));
    TEST_ASSERT_TRUE(ConfigManager::is_dirty());
    
    // Discard changes (typical UI "Cancel" action)
    ConfigManager::discard_changes();
    TEST_ASSERT_FALSE(ConfigManager::is_dirty());
    
    // Verify changes were discarded
    nlohmann::json value1 = ConfigManager::get("temp.change1");
    nlohmann::json value2 = ConfigManager::get("temp.change2");
    TEST_ASSERT_TRUE(value1.is_null());
    TEST_ASSERT_TRUE(value2.is_null());
    
    ESP_LOGI(TAG, "Discard changes: PASS");
}

// Test import/export (configuration management operations)
void test_config_manager_import_export() {
    ESP_LOGI(TAG, "Testing import/export operations...");
    
    // Create test configuration
    nlohmann::json test_config = {
        {"imported_section", {
            {"enabled", true},
            {"value", 999}
        }}
    };
    
    // Import configuration
    std::string json_str = test_config.dump();
    TEST_ASSERT_EQUAL(ESP_OK, ConfigManager::import_config(json_str));
    TEST_ASSERT_TRUE(ConfigManager::is_dirty());
    
    // Verify imported data
    nlohmann::json imported = ConfigManager::get("imported_section.enabled");
    TEST_ASSERT_TRUE(imported.get<bool>());
    
    // Export configuration
    std::string exported = ConfigManager::export_config();
    TEST_ASSERT_TRUE(exported.length() > 100);  // Should be substantial JSON
    
    ESP_LOGI(TAG, "Import/export: PASS");
}

void test_config_manager_simple_health_check(void) {
    ESP_LOGI(TAG, "Testing simple health monitoring...");
    
    // Test basic health check
    ConfigManager::ConfigHealth health = ConfigManager::get_config_health();
    TEST_ASSERT_TRUE(health.startup_successful);
    TEST_ASSERT_TRUE(health.all_modules_present);
    TEST_ASSERT_TRUE(health.validation_passed);
    TEST_ASSERT_TRUE(health.save_working);
    TEST_ASSERT_EQUAL(ESP_OK, health.last_error_code);
    TEST_ASSERT_GREATER_THAN(0, health.config_size_bytes);
    
    // Test diagnostics
    ConfigManager::ConfigDiagnostics diagnostics = ConfigManager::get_diagnostics();
    TEST_ASSERT_TRUE(diagnostics.all_modules_loaded);
    TEST_ASSERT_TRUE(diagnostics.schema_validation_passed);
    TEST_ASSERT_GREATER_THAN(0, diagnostics.config_version);
    TEST_ASSERT_GREATER_THAN(0, diagnostics.total_keys_count);
    TEST_ASSERT_EQUAL(0, diagnostics.missing_modules.size());
    
    ESP_LOGI(TAG, "Health check results:");
    ESP_LOGI(TAG, "  All modules present: %s", health.all_modules_present ? "YES" : "NO");
    ESP_LOGI(TAG, "  Validation passed: %s", health.validation_passed ? "YES" : "NO");
    ESP_LOGI(TAG, "  Config size: %lu bytes", health.config_size_bytes);
    ESP_LOGI(TAG, "  Total keys: %zu", diagnostics.total_keys_count);
    ESP_LOGI(TAG, "  Config version: %lu", diagnostics.config_version);
    
    ESP_LOGI(TAG, "Simple health monitoring: PASS");
}

// Test group runner (runs once - like real system startup)
void test_config_manager_group(void) {
    ESP_LOGI(TAG, "Starting ConfigManager tests (production pattern)...");
    
    // Initialize NVS (still needed for some ESP32 components)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Run tests in order that reflects real usage
    RUN_TEST(test_config_manager_production_startup);
    RUN_TEST(test_config_manager_runtime_operations);
    RUN_TEST(test_config_manager_persistence);
    RUN_TEST(test_config_manager_discard_changes);
    RUN_TEST(test_config_manager_import_export);
    RUN_TEST(test_config_manager_simple_health_check);
    
    // Clean up
    ConfigManager::deinit();
    
    ESP_LOGI(TAG, "ConfigManager test group completed successfully!");
}
