/**
 * @file test_module_manager.cpp
 * @brief Unit tests for ModuleManager
 */

#include <gtest/gtest.h>
#include "module_manager.h"
#include "base_module.h"
#include <atomic>

// Mock module for testing
class MockModule : public BaseModule {
public:
    explicit MockModule(const char* name) : name_(name) {}
    
    const char* get_name() const override { return name_; }
    
    esp_err_t init() override {
        init_count_++;
        return init_result_;
    }
    
    void update() override {
        update_count_++;
        if (update_callback_) {
            update_callback_();
        }
    }
    
    void stop() override {
        stop_count_++;
    }
    
    void configure(const nlohmann::json& config) override {        config_ = config;
        configured_ = true;
    }
    
    bool is_healthy() const override { return is_healthy_; }
    uint8_t get_health_score() const override { return health_score_; }
    uint32_t get_max_update_time_us() const override { return max_update_time_; }
    
    // Test helpers
    int init_count_ = 0;
    int update_count_ = 0;
    int stop_count_ = 0;
    bool configured_ = false;
    nlohmann::json config_;
    
    esp_err_t init_result_ = ESP_OK;
    bool is_healthy_ = true;
    uint8_t health_score_ = 100;
    uint32_t max_update_time_ = 2000;
    
    std::function<void()> update_callback_;
    
private:
    const char* name_;
};

class ModuleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        ModuleManager::init();
    }    
    void TearDown() override {
        ModuleManager::shutdown_all();
    }
};

// Test module registration and initialization
TEST_F(ModuleManagerTest, RegisterAndInit) {
    auto module = std::make_unique<MockModule>("test_module");
    auto* module_ptr = module.get();
    
    // Register module
    EXPECT_EQ(ESP_OK, ModuleManager::register_module(
        std::move(module), ModuleType::STANDARD));
    
    // Find module
    EXPECT_EQ(module_ptr, ModuleManager::find_module("test_module"));
    
    // Initialize all modules
    EXPECT_EQ(ESP_OK, ModuleManager::init_all());
    EXPECT_EQ(1, module_ptr->init_count_);
}

// Test module priority ordering
TEST_F(ModuleManagerTest, PriorityOrdering) {
    std::vector<std::string> init_order;
    
    // Create modules with different priorities
    auto critical = std::make_unique<MockModule>("critical");
    critical->update_callback_ = [&]() { init_order.push_back("critical"); };
        
    auto high = std::make_unique<MockModule>("high");
    high->update_callback_ = [&]() { init_order.push_back("high"); };
    
    auto standard = std::make_unique<MockModule>("standard");
    standard->update_callback_ = [&]() { init_order.push_back("standard"); };
    
    auto low = std::make_unique<MockModule>("low");
    low->update_callback_ = [&]() { init_order.push_back("low"); };
    
    // Register in random order
    ModuleManager::register_module(std::move(standard), ModuleType::STANDARD);
    ModuleManager::register_module(std::move(low), ModuleType::LOW);
    ModuleManager::register_module(std::move(critical), ModuleType::CRITICAL);
    ModuleManager::register_module(std::move(high), ModuleType::HIGH);
    
    // Initialize and update
    ModuleManager::init_all();
    ModuleManager::tick_all(100);
    
    // Verify priority order
    ASSERT_EQ(4, init_order.size());
    EXPECT_EQ("critical", init_order[0]);
    EXPECT_EQ("high", init_order[1]);
    EXPECT_EQ("standard", init_order[2]);
    EXPECT_EQ("low", init_order[3]);
}

// Test configuration
TEST_F(ModuleManagerTest, Configuration) {
    auto module = std::make_unique<MockModule>("config_test");
    auto* module_ptr = module.get();
    
    ModuleManager::register_module(std::move(module));
    
    // Configure with JSON
    nlohmann::json config = {
        {"config_test", {
            {"param1", "value1"},
            {"param2", 42}
        }}
    };
    
    ModuleManager::configure_all(config);
    
    EXPECT_TRUE(module_ptr->configured_);
    EXPECT_EQ("value1", module_ptr->config_["param1"]);
    EXPECT_EQ(42, module_ptr->config_["param2"]);
}

// Test module enable/disable
TEST_F(ModuleManagerTest, EnableDisable) {
    auto module = std::make_unique<MockModule>("toggle_test");
    auto* module_ptr = module.get();
    
    ModuleManager::register_module(std::move(module));
    ModuleManager::init_all();    
    // Module should be enabled by default
    EXPECT_TRUE(ModuleManager::is_module_enabled("toggle_test"));
    
    // Update should be called
    module_ptr->update_count_ = 0;
    ModuleManager::tick_all();
    EXPECT_GT(module_ptr->update_count_, 0);
    
    // Disable module
    EXPECT_EQ(ESP_OK, ModuleManager::disable_module("toggle_test"));
    EXPECT_FALSE(ModuleManager::is_module_enabled("toggle_test"));
    
    // Update should not be called when disabled
    module_ptr->update_count_ = 0;
    ModuleManager::tick_all();
    EXPECT_EQ(0, module_ptr->update_count_);
    
    // Re-enable
    EXPECT_EQ(ESP_OK, ModuleManager::enable_module("toggle_test"));
    EXPECT_TRUE(ModuleManager::is_module_enabled("toggle_test"));
}

// Test health monitoring
TEST_F(ModuleManagerTest, HealthMonitoring) {
    auto healthy = std::make_unique<MockModule>("healthy");
    healthy->is_healthy_ = true;
    healthy->health_score_ = 100;    
    auto degraded = std::make_unique<MockModule>("degraded");
    degraded->is_healthy_ = true;
    degraded->health_score_ = 70;
    
    auto unhealthy = std::make_unique<MockModule>("unhealthy");
    unhealthy->is_healthy_ = false;
    unhealthy->health_score_ = 30;
    
    ModuleManager::register_module(std::move(healthy));
    ModuleManager::register_module(std::move(degraded));
    ModuleManager::register_module(std::move(unhealthy));
    
    ModuleManager::init_all();
    
    auto report = ModuleManager::get_health_report();
    
    EXPECT_EQ(3, report.total_modules);
    EXPECT_EQ(2, report.healthy_modules);  // healthy + degraded
    EXPECT_EQ(0, report.degraded_modules);
    EXPECT_EQ(0, report.error_modules);
    
    // System health should be average of all modules
    uint8_t expected_health = (100 + 70 + 30) / 3;
    EXPECT_NEAR(expected_health, report.system_health_score, 1);
}

// Test module reload
TEST_F(ModuleManagerTest, ModuleReload) {
    auto module = std::make_unique<MockModule>("reload_test");
    auto* module_ptr = module.get();
    
    ModuleManager::register_module(std::move(module));
    ModuleManager::init_all();
    
    // Initial state
    EXPECT_EQ(1, module_ptr->init_count_);
    EXPECT_EQ(0, module_ptr->stop_count_);
    
    // Reload with new config
    nlohmann::json new_config = {{"new_param", "new_value"}};
    EXPECT_EQ(ESP_OK, ModuleManager::reload_module("reload_test", new_config));
    
    // Should have stopped and re-initialized
    EXPECT_EQ(1, module_ptr->stop_count_);
    EXPECT_EQ(2, module_ptr->init_count_);
    EXPECT_EQ("new_value", module_ptr->config_["new_param"]);
}

// Test error handling
TEST_F(ModuleManagerTest, ErrorHandling) {
    auto failing = std::make_unique<MockModule>("failing");
    failing->init_result_ = ESP_ERR_NO_MEM;
    
    ModuleManager::register_module(std::move(failing), ModuleType::STANDARD);    
    // Critical module that must succeed
    auto critical = std::make_unique<MockModule>("critical_ok");
    critical->init_result_ = ESP_OK;
    
    ModuleManager::register_module(std::move(critical), ModuleType::CRITICAL);
    
    // Init should succeed because critical module is OK
    EXPECT_EQ(ESP_OK, ModuleManager::init_all());
    
    // Check module states
    ModuleState state;
    EXPECT_EQ(ESP_OK, ModuleManager::get_module_state("critical_ok", state));
    EXPECT_EQ(ModuleState::INITIALIZED, state);
    
    // Failing module should be in error state
    EXPECT_EQ(ESP_OK, ModuleManager::get_module_state("failing", state));
    EXPECT_EQ(ModuleState::ERROR, state);
}

// Test update time tracking
TEST_F(ModuleManagerTest, UpdateTimeTracking) {
    auto slow = std::make_unique<MockModule>("slow_module");
    slow->max_update_time_ = 1000;  // 1ms budget
    slow->update_callback_ = []() {
        // Simulate slow update
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    };    
    ModuleManager::register_module(std::move(slow));
    ModuleManager::init_all();
    
    // Run several updates
    for (int i = 0; i < 5; i++) {
        ModuleManager::tick_all();
    }
    
    // Get statistics
    ModuleManager::ModuleStats stats;
    EXPECT_EQ(ESP_OK, ModuleManager::get_module_stats("slow_module", stats));
    
    EXPECT_EQ(5, stats.update_count);
    EXPECT_GT(stats.avg_update_time_us, 400);  // Should be around 500us
    EXPECT_GT(stats.max_update_time_us, 400);
}