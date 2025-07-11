/**
 * @file test_manifest_integration.cpp
 * @brief Integration test for manifest-driven module system
 * 
 * This test demonstrates and verifies:
 * - Module loading from manifests
 * - Event system integration
 * - Dependency resolution
 * - API registration
 */

#include "unity.h"
#include "module_manager.h"
#include "manifest_reader.h"
#include "module_factory.h"
#include "event_bus.h"
#include "event_validator.h"
#include "event_helpers.h"
#include "esp_log.h"

using namespace ModESP;

static const char* TAG = "TestManifest";

// Test module for integration testing
class TestModule : public BaseModule {
public:
    const char* get_name() const override { return "TestModule"; }
    
    esp_err_t init() override {
        ESP_LOGI(TAG, "TestModule initializing");
        init_called = true;
        
        // Subscribe to events
        sub_handle = EventSubscriber::onSystemHeartbeat(
            [this](size_t modules, uint8_t health, uint32_t uptime) {
                heartbeat_received = true;
                last_health = health;
            });
        
        return ESP_OK;
    }
    
    void update() override {
        update_count++;
        
        if (update_count % 5 == 0) {
            // Publish event every 5 updates
            EventPublisher::publishSensorReadingUpdated(
                "test_sensor", 
                25.5f + (update_count % 10), 
                "°C"
            );
        }
    }
    
    void stop() override {
        if (sub_handle != 0) {
            EventBus::unsubscribe(sub_handle);
        }
        stop_called = true;
    }
    
    bool is_healthy() const override { return true; }
    uint8_t get_health_score() const override { return 95; }
    
    // Test verification helpers
    bool init_called = false;
    bool stop_called = false;
    bool heartbeat_received = false;
    uint32_t update_count = 0;
    uint8_t last_health = 0;
    EventBus::SubscriptionHandle sub_handle = 0;
};

// Register test module
REGISTER_MODULE(TestModule);

// Test fixture
static TestModule* test_module_ptr = nullptr;

void setUp(void) {
    // Initialize systems
    ModuleManager::init();
    EventBus::init();
}

void tearDown(void) {
    // Cleanup
    ModuleManager::shutdown_all();
    test_module_ptr = nullptr;
}

// Test: Manifest reader initialization
TEST_CASE("manifest_reader_init", "[manifest]") {
    auto& reader = ManifestReader::getInstance();
    
    // Should initialize successfully
    TEST_ASSERT_EQUAL(ESP_OK, reader.init());
    
    // Should have loaded modules
    auto manifests = reader.getAllModuleManifests();
    TEST_ASSERT_GREATER_THAN(0, manifests.size());
    
    // Log loaded modules
    for (const auto& manifest : manifests) {
        ESP_LOGI(TAG, "Found module: %s v%s", 
                 manifest->getName(), 
                 manifest->getVersion());
    }
}

// Test: Module factory registration
TEST_CASE("module_factory", "[manifest]") {
    auto& factory = ModuleFactory::getInstance();
    
    // Should have registered modules
    auto registered = factory.getRegisteredModules();
    TEST_ASSERT_GREATER_THAN(0, registered.size());
    
    // Should include our test module
    TEST_ASSERT_TRUE(factory.hasModule("TestModule"));
    
    // Should create module instance
    auto module = factory.createModule("TestModule");
    TEST_ASSERT_NOT_NULL(module.get());
    TEST_ASSERT_EQUAL_STRING("TestModule", module->get_name());
}

// Test: Dependency resolution
TEST_CASE("dependency_resolution", "[manifest]") {
    auto& reader = ManifestReader::getInstance();
    
    // Get module load order
    std::vector<std::string> loadOrder;
    TEST_ASSERT_EQUAL(ESP_OK, reader.getModuleLoadOrder(loadOrder));
    
    // Should have determined order
    TEST_ASSERT_GREATER_THAN(0, loadOrder.size());
    
    ESP_LOGI(TAG, "Module load order:");
    for (size_t i = 0; i < loadOrder.size(); i++) {
        ESP_LOGI(TAG, "  %d. %s", i + 1, loadOrder[i].c_str());
    }
    
    // Verify dependencies come before dependents
    auto modulePos = [&loadOrder](const std::string& name) {
        auto it = std::find(loadOrder.begin(), loadOrder.end(), name);
        return it != loadOrder.end() ? 
               std::distance(loadOrder.begin(), it) : -1;
    };
    
    // SharedState should come before modules that depend on it
    for (const auto& module : loadOrder) {
        auto manifest = reader.getModuleManifest(module);
        if (manifest) {
            auto deps = manifest->getDependencies();
            for (const auto& dep : deps) {
                if (modulePos(dep) >= 0) {
                    TEST_ASSERT_LESS_THAN(modulePos(module), modulePos(dep));
                }
            }
        }
    }
}

// Test: Event validation
TEST_CASE("event_validation", "[manifest]") {
    auto& validator = EventValidator::getInstance();
    
    // Initialize validator
    TEST_ASSERT_EQUAL(ESP_OK, validator.init());
    
    // Should validate generated events
    TEST_ASSERT_TRUE(validator.isValidEvent("sensor.error"));
    TEST_ASSERT_TRUE(validator.isValidEvent("system.heartbeat"));
    
    // Should reject invalid events (when validation enabled)
    validator.setValidationEnabled(true);
    TEST_ASSERT_FALSE(validator.isValidEvent("invalid.event.name"));
    
    // Should allow when validation disabled
    validator.setValidationEnabled(false);
    TEST_ASSERT_TRUE(validator.isValidEvent("invalid.event.name"));
    
    // Should support dynamic patterns
    validator.setValidationEnabled(true);
    validator.allowDynamicPattern("test.*");
    TEST_ASSERT_TRUE(validator.isValidEvent("test.custom.event"));
}

// Test: Module registration from manifests
TEST_CASE("register_from_manifests", "[manifest]") {
    // Register test module manually first
    auto module = std::make_unique<TestModule>();
    test_module_ptr = module.get();
    TEST_ASSERT_EQUAL(ESP_OK, 
        ModuleManager::register_module(std::move(module)));
    
    // Register remaining modules from manifests
    TEST_ASSERT_EQUAL(ESP_OK, 
        ModuleManager::register_modules_from_manifests());
    
    // Should have multiple modules registered
    auto allModules = ModuleManager::get_all_modules();
    TEST_ASSERT_GREATER_THAN(1, allModules.size());
    
    // Find our test module
    auto testModule = ModuleManager::find_module("TestModule");
    TEST_ASSERT_NOT_NULL(testModule);
    TEST_ASSERT_EQUAL(test_module_ptr, testModule);
}

// Test: Type-safe event publishing and subscribing
TEST_CASE("type_safe_events", "[manifest]") {
    bool event_received = false;
    float received_value = 0;
    std::string received_unit;
    
    // Subscribe using type-safe helper
    auto handle = EventSubscriber::onSensorReading(
        [&](const std::string& role, float value, const std::string& unit) {
            event_received = true;
            received_value = value;
            received_unit = unit;
        });
    
    // Publish using type-safe helper
    EventPublisher::publishSensorReadingUpdated("test", 42.5f, "°C");
    
    // Process events
    EventBus::process(10);
    
    // Verify event received
    TEST_ASSERT_TRUE(event_received);
    TEST_ASSERT_EQUAL_FLOAT(42.5f, received_value);
    TEST_ASSERT_EQUAL_STRING("°C", received_unit.c_str());
    
    // Cleanup
    EventBus::unsubscribe(handle);
}

// Test: Complete integration flow
TEST_CASE("complete_integration", "[manifest]") {
    // 1. Register modules
    auto module = std::make_unique<TestModule>();
    test_module_ptr = module.get();
    ModuleManager::register_module(std::move(module));
    ModuleManager::register_modules_from_manifests();
    
    // 2. Configure modules
    nlohmann::json config = {
        {"test", {{"enabled", true}}}
    };
    ModuleManager::configure_all(config);
    
    // 3. Initialize modules
    TEST_ASSERT_EQUAL(ESP_OK, ModuleManager::init_all());
    TEST_ASSERT_TRUE(test_module_ptr->init_called);
    
    // 4. Run update cycles
    for (int i = 0; i < 10; i++) {
        ModuleManager::tick_all(5);
        EventBus::process(5);
    }
    
    // 5. Verify module ran
    TEST_ASSERT_GREATER_THAN(5, test_module_ptr->update_count);
    
    // 6. Publish heartbeat
    auto report = ModuleManager::get_health_report();
    EventPublisher::publishSystemHeartbeat(
        report.total_modules,
        report.system_health_score,
        10
    );
    EventBus::process(10);
    
    // 7. Verify heartbeat received
    TEST_ASSERT_TRUE(test_module_ptr->heartbeat_received);
    
    // 8. Shutdown
    ModuleManager::shutdown_all();
    TEST_ASSERT_TRUE(test_module_ptr->stop_called);
}

// Test: Generated API registration
TEST_CASE("api_registration", "[manifest]") {
    // This would require a mock RPC registrar
    // For now, just verify the manifest has APIs
    
    auto& reader = ManifestReader::getInstance();
    auto apis = reader.getAllAPIMethods();
    
    TEST_ASSERT_GREATER_THAN(0, apis.size());
    
    ESP_LOGI(TAG, "Found %d API methods:", apis.size());
    for (const auto& api : apis) {
        auto info = reader.getAPIInfo(api);
        if (info) {
            ESP_LOGI(TAG, "  %s (module: %s, access: %d)",
                     api.c_str(), 
                     info->module,
                     (int)info->access_level);
        }
    }
}

// Unity test runner
extern "C" void app_main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(manifest_reader_init);
    RUN_TEST(module_factory);
    RUN_TEST(dependency_resolution);
    RUN_TEST(event_validation);
    RUN_TEST(register_from_manifests);
    RUN_TEST(type_safe_events);
    RUN_TEST(complete_integration);
    RUN_TEST(api_registration);
    
    UNITY_END();
}
