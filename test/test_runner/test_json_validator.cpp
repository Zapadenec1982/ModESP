/**
 * @file test_json_validator.cpp
 * @brief Unit tests for JsonValidator component
 */

#include "unity.h"
#include "json_validator.h"
#include "esp_log.h"

static const char* TAG = "TestJsonValidator";

using namespace ModESP;

// Test cases
void test_json_validator_validate_type() {
    auto& validator = JsonValidator::instance();
    std::vector<ValidationError> errors;
    
    // Test string validation
    nlohmann::json str_schema = {{"type", "string"}};
    validator.load_schema("string_test", str_schema);
    
    TEST_ASSERT_TRUE(validator.validate(nlohmann::json("test"), "string_test", errors));
    TEST_ASSERT_FALSE(validator.validate(nlohmann::json(123), "string_test", errors));
    
    // Test number validation
    nlohmann::json num_schema = {{"type", "number"}};
    validator.load_schema("number_test", num_schema);
    
    TEST_ASSERT_TRUE(validator.validate(nlohmann::json(42), "number_test", errors));
    TEST_ASSERT_TRUE(validator.validate(nlohmann::json(3.14), "number_test", errors));
    TEST_ASSERT_FALSE(validator.validate(nlohmann::json("not a number"), "number_test", errors));
}

void test_json_validator_validate_range() {
    auto& validator = JsonValidator::instance();
    std::vector<ValidationError> errors;
    
    nlohmann::json schema = {
        {"type", "number"},
        {"minimum", 0},
        {"maximum", 100}
    };
    
    validator.load_schema("range_test", schema);
    
    TEST_ASSERT_TRUE(validator.validate(nlohmann::json(50), "range_test", errors));
    TEST_ASSERT_TRUE(validator.validate(nlohmann::json(0), "range_test", errors));
    TEST_ASSERT_TRUE(validator.validate(nlohmann::json(100), "range_test", errors));
    TEST_ASSERT_FALSE(validator.validate(nlohmann::json(-1), "range_test", errors));
    TEST_ASSERT_FALSE(validator.validate(nlohmann::json(101), "range_test", errors));
}

void test_json_validator_validate_object() {
    auto& validator = JsonValidator::instance();
    std::vector<ValidationError> errors;
    
    nlohmann::json schema = {
        {"type", "object"},
        {"required", {"name", "value"}},
        {"properties", {
            {"name", {{"type", "string"}}},
            {"value", {{"type", "number"}}},
            {"optional", {{"type", "boolean"}}}
        }}
    };
    
    validator.load_schema("object_test", schema);
    
    // Valid object
    nlohmann::json valid = {
        {"name", "test"},
        {"value", 42}
    };
    TEST_ASSERT_TRUE(validator.validate(valid, "object_test", errors));
    
    // Valid with optional
    valid["optional"] = true;
    TEST_ASSERT_TRUE(validator.validate(valid, "object_test", errors));
    
    // Missing required field
    nlohmann::json invalid = {{"name", "test"}};
    TEST_ASSERT_FALSE(validator.validate(invalid, "object_test", errors));
}

// Test group runner
void test_json_validator_group(void) {
    RUN_TEST(test_json_validator_validate_type);
    RUN_TEST(test_json_validator_validate_range);
    RUN_TEST(test_json_validator_validate_object);
}
