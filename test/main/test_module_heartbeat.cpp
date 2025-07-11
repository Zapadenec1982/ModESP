/**
 * @file test_module_heartbeat.cpp
 * @brief Unit tests for ModuleHeartbeat
 */

#include "unity.h"
#include "module_heartbeat.h"
#include "module_manager.h"
#include <memory>

// Test fixture
static std::unique_ptr<ModuleHeartbeat> heartbeat;

void setUp(void) {
    heartbeat = std::make_unique<ModuleHeartbeat>();
}

void tearDown(void) {
    heartbeat.reset();
}

// Test basic initialization
void test_heartbeat_init(void) {
    TEST_ASSERT_EQUAL(ESP_OK, heartbeat->init());
    TEST_ASSERT_EQUAL_STRING("ModuleHeartbeat", heartbeat->get_name());
}

// Test module registration
void test_heartbeat_register_module(void) {
    heartbeat->init();
    
    // Register test module
    heartbeat->register_module("TestModule", ModuleType::STANDARD);
    
    // Should be alive immediately after registration
    TEST_ASSERT_TRUE(heartbeat->is_module_alive("TestModule"));
    TEST_ASSERT_EQUAL(0, heartbeat->get_restart_count("TestModule"));
}

// Test heartbeat update
void test_heartbeat_update(void) {
    heartbeat->init();
    heartbeat->register_module("TestModule", ModuleType::STANDARD);
    
    // Update heartbeat
    heartbeat->update_heartbeat("TestModule");
    TEST_ASSERT_TRUE(heartbeat->is_module_alive("TestModule"));
}

// Test module unregistration
void test_heartbeat_unregister_module(void) {
    heartbeat->init();
    heartbeat->register_module("TestModule", ModuleType::STANDARD);
    
    // Unregister
    heartbeat->unregister_module("TestModule");
    
    // Should not be alive after unregistration
    TEST_ASSERT_FALSE(heartbeat->is_module_alive("TestModule"));
}

// Test health score
void test_heartbeat_health_score(void) {
    heartbeat->init();
    
    // Should start at 100%
    TEST_ASSERT_EQUAL(100, heartbeat->get_health_score());
    TEST_ASSERT_TRUE(heartbeat->is_healthy());
}

// Test configuration
void test_heartbeat_configuration(void) {
    nlohmann::json config = {
        {"heartbeat", {
            {"enabled", true},
            {"check_interval_ms", 1000},
            {"auto_restart_enabled", false},
            {"timeouts", {
                {"critical_ms", 2000},
                {"standard_ms", 10000},
                {"background_ms", 60000}
            }}
        }}
    };
    
    heartbeat->configure(config);
    heartbeat->init();
    
    // Verify configuration was applied
    TEST_ASSERT_TRUE(heartbeat->is_healthy());
}

// Run all tests
void run_module_heartbeat_tests(void) {
    RUN_TEST(test_heartbeat_init);
    RUN_TEST(test_heartbeat_register_module);
    RUN_TEST(test_heartbeat_update);
    RUN_TEST(test_heartbeat_unregister_module);
    RUN_TEST(test_heartbeat_health_score);
    RUN_TEST(test_heartbeat_configuration);
}
// Test mutex handling
void test_heartbeat_mutex_robustness(void) {
    // Create heartbeat without proper initialization to test mutex checks
    ModuleHeartbeat* hb = new ModuleHeartbeat();
    
    // These operations should handle null mutex gracefully
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, hb->init());
    
    delete hb;
}

// Test timeout-based operations
void test_heartbeat_timeout_operations(void) {
    heartbeat->init();
    heartbeat->register_module("TestModule", ModuleType::STANDARD);
    
    // Test the example timeout-based method
    TEST_ASSERT_TRUE(heartbeat->try_get_module_status("TestModule", 100));
    TEST_ASSERT_FALSE(heartbeat->try_get_module_status("NonExistentModule", 100));
}

// Run all tests
void run_module_heartbeat_tests(void) {
    RUN_TEST(test_heartbeat_init);
    RUN_TEST(test_heartbeat_register_module);
    RUN_TEST(test_heartbeat_update);
    RUN_TEST(test_heartbeat_unregister_module);
    RUN_TEST(test_heartbeat_health_score);
    RUN_TEST(test_heartbeat_configuration);
    RUN_TEST(test_heartbeat_mutex_robustness);
    RUN_TEST(test_heartbeat_timeout_operations);
}