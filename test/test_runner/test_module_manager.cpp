/**
 * @file test_module_manager.cpp
 * @brief Unit tests for ModuleManager component
 */

#include "unity.h"
#include "module_manager.h"
#include "base_module.h"
#include "esp_log.h"
#include <memory>

static const char* TAG = "TestModuleManager";

// Mock module for testing
class TestModule : public BaseModule {
private:
    std::string m_name;
    bool m_initialized = false;
    int m_update_count = 0;
    
public:
    TestModule(const char* name) : m_name(name) {}
    
    const char* get_name() const override { return m_name.c_str(); }
    
    esp_err_t init() override {
        m_initialized = true;
        return ESP_OK;
    }
    
    void stop() override {
        m_initialized = false;
    }
    
    void update() override {
        m_update_count++;
    }
    
    void register_rpc(IJsonRpcRegistrar& rpc) override {
        // No RPC methods for test module
    }
    
    bool is_initialized() const { return m_initialized; }
    int get_update_count() const { return m_update_count; }
};

// Helper functions
static void reset_test_state() {
    ModuleManager::shutdown_all();
    ModuleManager::init();
}

// Test cases
void test_module_manager_init() {
    TEST_ASSERT_EQUAL(ESP_OK, ModuleManager::init());
}

void test_module_manager_register() {
    reset_test_state();
    
    auto module = std::make_unique<TestModule>("test_module");
    TEST_ASSERT_EQUAL(ESP_OK, 
        ModuleManager::register_module(std::move(module), ModuleType::STANDARD));
    
    // Try to register duplicate
    auto duplicate = std::make_unique<TestModule>("test_module");
    TEST_ASSERT_NOT_EQUAL(ESP_OK, 
        ModuleManager::register_module(std::move(duplicate), ModuleType::STANDARD));
}

void test_module_manager_find() {
    reset_test_state();
    
    auto module = std::make_unique<TestModule>("findable");
    ModuleManager::register_module(std::move(module), ModuleType::STANDARD);
    
    // Find existing module
    auto found = ModuleManager::find_module("findable");
    TEST_ASSERT_NOT_NULL(found);
    TEST_ASSERT_EQUAL_STRING("findable", found->get_name());
    
    // Find non-existing module
    auto not_found = ModuleManager::find_module("nonexistent");
    TEST_ASSERT_NULL(not_found);
}

void test_module_manager_init_all() {
    reset_test_state();
    
    // Register multiple modules
    auto module1 = std::make_unique<TestModule>("module1");
    auto module2 = std::make_unique<TestModule>("module2");
    
    TestModule* ptr1 = module1.get();
    TestModule* ptr2 = module2.get();
    
    ModuleManager::register_module(std::move(module1), ModuleType::HIGH);
    ModuleManager::register_module(std::move(module2), ModuleType::STANDARD);
    
    // Initialize all
    TEST_ASSERT_EQUAL(ESP_OK, ModuleManager::init_all());
    
    // Verify initialization
    TEST_ASSERT_TRUE(ptr1->is_initialized());
    TEST_ASSERT_TRUE(ptr2->is_initialized());
}

void test_module_manager_enable_disable() {
    reset_test_state();
    
    auto module = std::make_unique<TestModule>("toggleable");
    ModuleManager::register_module(std::move(module), ModuleType::STANDARD);
    ModuleManager::init_all();
    
    // Initially enabled
    TEST_ASSERT_TRUE(ModuleManager::is_module_enabled("toggleable"));
    
    // Disable
    TEST_ASSERT_EQUAL(ESP_OK, ModuleManager::disable_module("toggleable"));
    TEST_ASSERT_FALSE(ModuleManager::is_module_enabled("toggleable"));
    
    // Enable
    TEST_ASSERT_EQUAL(ESP_OK, ModuleManager::enable_module("toggleable"));
    TEST_ASSERT_TRUE(ModuleManager::is_module_enabled("toggleable"));
}

// Test group runner
void test_module_manager_group(void) {
    RUN_TEST(test_module_manager_init);
    RUN_TEST(test_module_manager_register);
    RUN_TEST(test_module_manager_find);
    RUN_TEST(test_module_manager_init_all);
    RUN_TEST(test_module_manager_enable_disable);
}
