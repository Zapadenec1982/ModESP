/**
 * @file test_event_bus.cpp
 * @brief Unit tests for EventBus
 */

#include <gtest/gtest.h>
#include "event_bus.h"
#include <thread>
#include <chrono>
#include <atomic>

class EventBusTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize EventBus before each test
        EventBus::init(32);
    }
    
    void TearDown() override {
        // Clean up after each test
        EventBus::clear();
    }
};

// Test basic publish and subscribe
TEST_F(EventBusTest, BasicPubSub) {
    bool handler_called = false;
    nlohmann::json received_data;
    
    // Subscribe to test event
    auto handle = EventBus::subscribe("test.event", 
        [&](const EventBus::Event& event) {
            handler_called = true;
            received_data = event.data;
        });
        
    // Publish event
    nlohmann::json test_data = {{"value", 42}, {"message", "test"}};
    EXPECT_EQ(ESP_OK, EventBus::publish("test.event", test_data));
    
    // Process events
    EventBus::process(10);
    
    // Verify handler was called
    EXPECT_TRUE(handler_called);
    EXPECT_EQ(test_data, received_data);
    
    // Cleanup
    EventBus::unsubscribe(handle);
}

// Test pattern matching
TEST_F(EventBusTest, PatternMatching) {
    int sensor_count = 0;
    int temp_count = 0;
    int all_count = 0;
    
    // Subscribe to different patterns
    auto h1 = EventBus::subscribe("sensor.*", 
        [&](const EventBus::Event& event) { sensor_count++; });
    
    auto h2 = EventBus::subscribe("sensor.temp.*", 
        [&](const EventBus::Event& event) { temp_count++; });
    
    auto h3 = EventBus::subscribe("", 
        [&](const EventBus::Event& event) { all_count++; });    
    // Publish various events
    EventBus::publish("sensor.temp.updated", {{"value", 25.5}});
    EventBus::publish("sensor.humidity.updated", {{"value", 65}});
    EventBus::publish("system.startup", {});
    
    // Process all events
    EventBus::process(10);
    
    // Verify counts
    EXPECT_EQ(2, sensor_count);  // sensor.temp and sensor.humidity
    EXPECT_EQ(1, temp_count);    // only sensor.temp
    EXPECT_EQ(3, all_count);     // all events
    
    // Cleanup
    EventBus::unsubscribe(h1);
    EventBus::unsubscribe(h2);
    EventBus::unsubscribe(h3);
}

// Test priority handling
TEST_F(EventBusTest, PriorityHandling) {
    std::vector<int> execution_order;
    
    // Subscribe to same event
    auto handle = EventBus::subscribe("priority.test",
        [&](const EventBus::Event& event) {
            execution_order.push_back(event.data["order"]);
        });    
    // Publish events with different priorities
    EventBus::publish("priority.test", {{"order", 3}}, EventBus::Priority::LOW);
    EventBus::publish("priority.test", {{"order", 1}}, EventBus::Priority::CRITICAL);
    EventBus::publish("priority.test", {{"order", 2}}, EventBus::Priority::HIGH);
    EventBus::publish("priority.test", {{"order", 4}}, EventBus::Priority::NORMAL);
    
    // Process all events
    EventBus::process(10);
    
    // Verify execution order (CRITICAL -> HIGH -> NORMAL -> LOW)
    ASSERT_EQ(4, execution_order.size());
    EXPECT_EQ(1, execution_order[0]);
    EXPECT_EQ(2, execution_order[1]);
    EXPECT_EQ(4, execution_order[2]);
    EXPECT_EQ(3, execution_order[3]);
    
    EventBus::unsubscribe(handle);
}

// Test queue overflow handling
TEST_F(EventBusTest, QueueOverflow) {
    // Fill the queue (initialized with size 32)
    for (int i = 0; i < 32; i++) {
        EXPECT_EQ(ESP_OK, EventBus::publish("overflow.test", {{"i", i}}));
    }
    
    // Next publish should fail
    EXPECT_EQ(ESP_ERR_NO_MEM, EventBus::publish("overflow.test", {{"i", 33}}));    
    // Verify queue is full
    EXPECT_TRUE(EventBus::is_queue_full());
    
    // Process some events to free space
    EventBus::process(5);
    
    // Should be able to publish again
    EXPECT_FALSE(EventBus::is_queue_full());
    EXPECT_EQ(ESP_OK, EventBus::publish("overflow.test", {{"i", 34}}));
}

// Test error handling in handlers
TEST_F(EventBusTest, ErrorHandling) {
    int success_count = 0;
    
    // Subscribe with handler that throws
    auto h1 = EventBus::subscribe("error.test",
        [&](const EventBus::Event& event) {
            if (event.data["fail"]) {
                throw std::runtime_error("Test error");
            }
            success_count++;
        });
    
    // Publish events
    EventBus::publish("error.test", {{"fail", false}});
    EventBus::publish("error.test", {{"fail", true}});  // This will throw
    EventBus::publish("error.test", {{"fail", false}});
    
    // Process events
    EventBus::process(10);    
    // Despite error, other events should be processed
    EXPECT_EQ(2, success_count);
    
    // Check error was recorded
    EXPECT_TRUE(EventBus::has_errors());
    EXPECT_GT(EventBus::get_error_count(), 0);
    
    EventBus::unsubscribe(h1);
}

// Test statistics
TEST_F(EventBusTest, Statistics) {
    auto handle = EventBus::subscribe("stats.test", 
        [](const EventBus::Event& event) {
            // Just consume event
        });
    
    // Reset stats
    EventBus::reset_stats();
    
    // Publish some events
    for (int i = 0; i < 10; i++) {
        EventBus::publish("stats.test", {{"i", i}});
    }
    
    // Process half
    EventBus::process(5);
    
    auto stats = EventBus::get_stats();
    EXPECT_EQ(10, stats.total_published);
    EXPECT_EQ(5, stats.total_processed);
    EXPECT_EQ(5, stats.queue_size);
    
    EventBus::unsubscribe(handle);
}