/**
 * @file test_shared_state.cpp
 * @brief Unit tests for SharedState component
 */

#include "unity.h"
#include "shared_state.h"
#include "esp_log.h"
#include <atomic>

static const char* TAG = "TestSharedState";

// Test fixtures
static std::atomic<int> callback_count{0};

// Helper functions
static void reset_test_state() {
    callback_count = 0;
    SharedState::clear();
}

// Test cases
void test_shared_state_init() {
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::init());
}

void test_shared_state_get_set() {
    reset_test_state();
    
    // Set value
    nlohmann::json value = {{"temperature", 25.5}};
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::set("sensor.temp", value));
    
    // Get value
    nlohmann::json retrieved;
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::get("sensor.temp", retrieved));
    TEST_ASSERT_FALSE(retrieved.is_null());
    
    double temp_value = retrieved["temperature"].get<double>();
    TEST_ASSERT_FLOAT_WITHIN(0.001, 25.5, temp_value);
}

void test_shared_state_subscribe() {
    reset_test_state();
    
    // Subscribe to changes
    auto handle = SharedState::subscribe("sensor.*", 
        [](const std::string& path, const nlohmann::json& value) {
            callback_count++;
        });
    
    // Make changes
    SharedState::set("sensor.temp", {{"value", 20}});
    SharedState::set("sensor.humidity", {{"value", 60}});
    SharedState::set("actuator.relay", {{"state", true}}); // Should not trigger
    
    // Allow callbacks to process
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Verify callbacks
    TEST_ASSERT_EQUAL(2, callback_count.load());
    
    // Unsubscribe
    SharedState::unsubscribe(handle);
}

// Test group runner  
void test_shared_state_group(void) {
    RUN_TEST(test_shared_state_init);
    RUN_TEST(test_shared_state_get_set);
    RUN_TEST(test_shared_state_subscribe);
    RUN_TEST(test_shared_state_increment);
    RUN_TEST(test_shared_state_typed_helpers);
}

void test_shared_state_increment() {
    reset_test_state();
    
    // Test increment on non-existing key
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::increment("metrics.counter", 5.0));
    
    // Verify value
    nlohmann::json value;
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::get("metrics.counter", value));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 5.0, value.get<double>());
    
    // Test increment on existing key
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::increment("metrics.counter", 3.0));
    
    // Verify updated value
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::get("metrics.counter", value));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 8.0, value.get<double>());
    
    // Test increment with default delta (1.0)
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::increment("metrics.counter"));
    
    // Verify default increment
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::get("metrics.counter", value));
    TEST_ASSERT_FLOAT_WITHIN(0.001, 9.0, value.get<double>());
    
    // Test increment on non-numeric value (should fail)
    SharedState::set("text.key", nlohmann::json{{"text", "hello"}});
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, SharedState::increment("text.key", 1.0));
}

void test_shared_state_typed_helpers() {
    reset_test_state();
    
    // Test typed setters
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::set_typed("test.float", 25.5f));
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::set_typed("test.int", 42));
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::set_typed("test.bool", true));
    TEST_ASSERT_EQUAL(ESP_OK, SharedState::set_typed("test.string", std::string("hello")));
    
    // Test typed getters
    auto float_val = SharedState::get_typed<float>("test.float");
    TEST_ASSERT_TRUE(float_val.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.001, 25.5f, float_val.value());
    
    auto int_val = SharedState::get_typed<int>("test.int");
    TEST_ASSERT_TRUE(int_val.has_value());
    TEST_ASSERT_EQUAL(42, int_val.value());
    
    auto bool_val = SharedState::get_typed<bool>("test.bool");
    TEST_ASSERT_TRUE(bool_val.has_value());
    TEST_ASSERT_TRUE(bool_val.value());
    
    auto string_val = SharedState::get_typed<std::string>("test.string");
    TEST_ASSERT_TRUE(string_val.has_value());
    TEST_ASSERT_EQUAL_STRING("hello", string_val.value().c_str());
    
    // Test non-existing key
    auto missing = SharedState::get_typed<float>("missing.key");
    TEST_ASSERT_FALSE(missing.has_value());
}
