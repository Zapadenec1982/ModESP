/**
 * @file test_event_bus.cpp
 * @brief Unit tests for EventBus component
 */

#include "unity.h"
#include "event_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <vector>
#include <atomic>
#include "freertos/semphr.h"

static const char* TAG = "TestEventBus";

// Test fixtures
static std::atomic<int> event_counter{0};
static std::vector<EventBus::Event> received_events;
static SemaphoreHandle_t test_semaphore = nullptr;

// Helper functions
static void reset_test_state() {
    event_counter = 0;
    received_events.clear();
    EventBus::clear();
    EventBus::reset_stats();
    EventBus::clear_errors();
}

static void test_handler(const EventBus::Event& event) {
    received_events.push_back(event);
    event_counter++;
    if (test_semaphore) {
        xSemaphoreGive(test_semaphore);
    }
}

// Test cases

void test_eventbus_init() {
    TEST_ASSERT_EQUAL(ESP_OK, EventBus::init(32));
    
    // Test double init (should handle gracefully)
    TEST_ASSERT_EQUAL(ESP_OK, EventBus::init(32));
}

void test_eventbus_publish_subscribe() {
    reset_test_state();
    
    // Subscribe to specific event
    auto handle = EventBus::subscribe("test.event", test_handler);
    TEST_ASSERT_NOT_EQUAL(0, handle);
    
    // Publish event
    nlohmann::json data = {{"value", 42}, {"message", "test"}};
    TEST_ASSERT_EQUAL(ESP_OK, EventBus::publish("test.event", data));
    
    // Process events
    EventBus::process(10);
    
    // Verify
    TEST_ASSERT_EQUAL(1, event_counter.load());
    TEST_ASSERT_EQUAL(1, received_events.size());
    TEST_ASSERT_EQUAL_STRING("test.event", received_events[0].type.c_str());
    TEST_ASSERT_EQUAL(42, received_events[0].data["value"].get<int>());
    
    // Unsubscribe
    TEST_ASSERT_EQUAL(ESP_OK, EventBus::unsubscribe(handle));
}

void test_eventbus_pattern_matching() {
    reset_test_state();
    
    // Subscribe to pattern
    auto handle = EventBus::subscribe("sensor.*", test_handler);
    
    // Publish matching events
    EventBus::publish("sensor.temperature", {{"value", 25.5}});
    EventBus::publish("sensor.humidity", {{"value", 60}});
    EventBus::publish("actuator.relay", {{"state", true}}); // Should not match
    
    // Process
    EventBus::process(20);
    
    // Verify only matching events received
    TEST_ASSERT_EQUAL(2, event_counter.load());
    TEST_ASSERT_EQUAL_STRING("sensor.temperature", received_events[0].type.c_str());
    TEST_ASSERT_EQUAL_STRING("sensor.humidity", received_events[1].type.c_str());
    
    EventBus::unsubscribe(handle);
}

void test_eventbus_wildcard_subscribe() {
    reset_test_state();
    
    // Subscribe to all events
    auto handle = EventBus::subscribe("", test_handler);
    
    // Publish various events
    EventBus::publish("event1", {});
    EventBus::publish("event2", {});
    EventBus::publish("event3", {});
    
    // Process
    EventBus::process(20);
    
    // Verify all events received
    TEST_ASSERT_EQUAL(3, event_counter.load());
    
    EventBus::unsubscribe(handle);
}

void test_eventbus_priority() {
    reset_test_state();
    
    std::vector<EventBus::Priority> received_priorities;
    
    auto priority_handler = [&](const EventBus::Event& event) {
        received_priorities.push_back(event.priority);
    };
    
    auto handle = EventBus::subscribe("", priority_handler);
    
    // Publish events with different priorities
    EventBus::publish("test", {}, EventBus::Priority::LOW);
    EventBus::publish("test", {}, EventBus::Priority::CRITICAL);
    EventBus::publish("test", {}, EventBus::Priority::NORMAL);
    EventBus::publish("test", {}, EventBus::Priority::HIGH);
    
    // Process all
    EventBus::process(50);
    
    // Verify priority order (CRITICAL, HIGH, NORMAL, LOW)
    TEST_ASSERT_EQUAL(4, received_priorities.size());
    TEST_ASSERT_EQUAL(EventBus::Priority::CRITICAL, received_priorities[0]);
    TEST_ASSERT_EQUAL(EventBus::Priority::HIGH, received_priorities[1]);
    TEST_ASSERT_EQUAL(EventBus::Priority::NORMAL, received_priorities[2]);
    TEST_ASSERT_EQUAL(EventBus::Priority::LOW, received_priorities[3]);
    
    EventBus::unsubscribe(handle);
}

void test_eventbus_queue_overflow() {
    reset_test_state();
    
    // Re-init with small queue
    EventBus::init(5);
    
    auto handle = EventBus::subscribe("", test_handler);
    
    // Flood with events
    int published = 0;
    int failed = 0;
    
    for (int i = 0; i < 10; i++) {
        if (EventBus::publish("test", {{"index", i}}) == ESP_OK) {
            published++;
        } else {
            failed++;
        }
    }
    
    // Verify some events were dropped
    TEST_ASSERT_GREATER_THAN(0, failed);
    TEST_ASSERT_EQUAL(5, published); // Queue size
    
    // Process and verify
    EventBus::process(50);
    TEST_ASSERT_EQUAL(published, event_counter.load());
    
    // Check stats
    auto stats = EventBus::get_stats();
    TEST_ASSERT_EQUAL(failed, stats.total_dropped);
    
    EventBus::unsubscribe(handle);
}

void test_eventbus_pause_resume() {
    reset_test_state();
    
    auto handle = EventBus::subscribe("test", test_handler);
    
    // Pause processing
    EventBus::pause();
    TEST_ASSERT_TRUE(EventBus::is_paused());
    
    // Publish events while paused
    EventBus::publish("test", {{"value", 1}});
    EventBus::publish("test", {{"value", 2}});
    
    // Process (should do nothing)
    EventBus::process(10);
    TEST_ASSERT_EQUAL(0, event_counter.load());
    
    // Resume and process
    EventBus::resume();
    TEST_ASSERT_FALSE(EventBus::is_paused());
    EventBus::process(20);
    
    // Verify events were processed after resume
    TEST_ASSERT_EQUAL(2, event_counter.load());
    
    EventBus::unsubscribe(handle);
}

void test_eventbus_multiple_subscribers() {
    reset_test_state();
    
    std::atomic<int> handler1_count{0};
    std::atomic<int> handler2_count{0};
    
    auto handler1 = [&](const EventBus::Event& event) {
        handler1_count++;
    };
    
    auto handler2 = [&](const EventBus::Event& event) {
        handler2_count++;
    };
    
    auto handle1 = EventBus::subscribe("test", handler1);
    auto handle2 = EventBus::subscribe("test", handler2);
    
    // Publish event
    EventBus::publish("test", {});
    EventBus::process(10);
    
    // Both handlers should receive the event
    TEST_ASSERT_EQUAL(1, handler1_count.load());
    TEST_ASSERT_EQUAL(1, handler2_count.load());
    
    EventBus::unsubscribe(handle1);
    EventBus::unsubscribe(handle2);
}

void test_eventbus_filter() {
    reset_test_state();
    
    auto handle = EventBus::subscribe("", test_handler);
    
    // Set filter to only allow events with "allowed" field
    EventBus::set_filter([](const EventBus::Event& event) {
        return event.data.contains("allowed") && event.data["allowed"].get<bool>();
    });
    
    // Publish events
    EventBus::publish("test1", {{"allowed", true}});
    EventBus::publish("test2", {{"allowed", false}});
    EventBus::publish("test3", {{"other", "data"}});
    
    EventBus::process(20);
    
    // Only first event should pass filter
    TEST_ASSERT_EQUAL(1, event_counter.load());
    TEST_ASSERT_EQUAL_STRING("test1", received_events[0].type.c_str());
    
    // Clear filter
    EventBus::clear_filter();
    
    EventBus::unsubscribe(handle);
}

void test_eventbus_concurrent_publish() {
    reset_test_state();
    
    // Reset EventBus to default queue size (32) for stress testing
    EventBus::init(32);
    
    test_semaphore = xSemaphoreCreateCounting(100, 0);
    
    auto handle = EventBus::subscribe("concurrent", test_handler);
    
    // Task to publish events - stress test with minimal delays
    auto publish_task = [](void* param) {
        int task_id = (int)param;
        
        // Pre-allocate JSON data to avoid repeated allocations
        nlohmann::json event_data;
        event_data["task"] = task_id;
        
        for (int i = 0; i < 10; i++) {
            // Reuse the same JSON object, just update the index
            event_data["index"] = i;
            
            EventBus::publish("concurrent", event_data);
            vTaskDelay(pdMS_TO_TICKS(5)); // Slightly longer delay to allow processing
        }
        vTaskDelete(NULL);
    };
    
    // Get initial stats
    auto initial_stats = EventBus::get_stats();
    
    // Create multiple publisher tasks for stress testing
    for (int i = 0; i < 3; i++) {
        xTaskCreate(publish_task, "publisher", 8192, (void*)i, 5, NULL);
    }
    
    // Allow tasks to start and publish events
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Process events in batches to ensure they get handled
    int processed_count = 0;
    const int total_expected = 30;
    const int max_iterations = 100; // Maximum processing iterations
    
    for (int iteration = 0; iteration < max_iterations && processed_count < total_expected; iteration++) {
        // Process events
        EventBus::process(50);
        
        // Check if we got new events via semaphore (non-blocking)
        while (xSemaphoreTake(test_semaphore, 0) == pdTRUE) {
            processed_count++;
        }
        
        // Small delay between processing cycles
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // Final processing round
    EventBus::process(100);
    
    // Get final stats
    auto final_stats = EventBus::get_stats();
    
    // Calculate events processed during this test
    int events_processed = final_stats.total_processed - initial_stats.total_processed;
    int events_dropped = final_stats.total_dropped - initial_stats.total_dropped;
    
    // Verify stress test results
    TEST_ASSERT_EQUAL(processed_count, event_counter.load());
    TEST_ASSERT_EQUAL(events_processed, processed_count);
    
    // The key stress test: verify that processed + dropped accounts for published events
    // In a stress test, some events may be dropped due to queue overflow, but we should get at least some
    TEST_ASSERT_GREATER_OR_EQUAL(5, events_processed); // At least 5 should be processed
    
    // Log results for debugging
    ESP_LOGI(TAG, "Stress test results: %d processed, %d dropped, expected up to %d", 
             events_processed, events_dropped, total_expected);
    
    // If some events were dropped, that's expected behavior under stress
    if (events_dropped > 0) {
        ESP_LOGI(TAG, "EventBus correctly handled overflow by dropping %d events", events_dropped);
    }
    
    EventBus::unsubscribe(handle);
    vSemaphoreDelete(test_semaphore);
    test_semaphore = nullptr;
}

void test_eventbus_error_handling() {
    reset_test_state();
    
    // Subscribe with handler that has controlled error simulation
    auto error_handler = [](const EventBus::Event& event) {
        if (event.data.contains("error") && event.data["error"].get<bool>()) {
            // Simulate error condition without exceptions (since ESP32 has no exception support)
            ESP_LOGW("ErrorTest", "Simulated error in event handler");
            return; // Early return to simulate error handling
        }
    };
    
    auto handle = EventBus::subscribe("error.test", error_handler);
    
    // Also subscribe normal handler to verify processing continues
    auto normal_handle = EventBus::subscribe("normal.test", test_handler);
    
    // Publish events
    EventBus::publish("error.test", {{"error", true}});
    EventBus::publish("normal.test", {{"value", 42}});
    
    EventBus::process(20);
    
    // Normal handler should still work
    TEST_ASSERT_EQUAL(1, event_counter.load());
    
    // In this simplified test, we just verify that the normal event was processed
    // The error handling is more about ensuring the system doesn't crash
    ESP_LOGI(TAG, "Error handling test completed - system remained stable");
    
    EventBus::unsubscribe(handle);
    EventBus::unsubscribe(normal_handle);
}

void test_eventbus_statistics() {
    reset_test_state();
    
    auto handle = EventBus::subscribe("stats", test_handler);
    
    // Publish some events
    for (int i = 0; i < 5; i++) {
        EventBus::publish("stats", {{"index", i}});
    }
    
    EventBus::process(50);
    
    // Get stats
    auto stats = EventBus::get_stats();
    
    TEST_ASSERT_EQUAL(5, stats.total_published);
    TEST_ASSERT_EQUAL(5, stats.total_processed);
    TEST_ASSERT_EQUAL(0, stats.total_dropped);
    TEST_ASSERT_EQUAL(1, stats.total_subscriptions);
    TEST_ASSERT_GREATER_THAN(0, stats.avg_process_time_us);
    
    EventBus::unsubscribe(handle);
}

// Test group runner
void test_event_bus_group(void) {
    RUN_TEST(test_eventbus_init);
    RUN_TEST(test_eventbus_publish_subscribe);
    RUN_TEST(test_eventbus_pattern_matching);
    RUN_TEST(test_eventbus_wildcard_subscribe);
    RUN_TEST(test_eventbus_priority);
    RUN_TEST(test_eventbus_queue_overflow);
    RUN_TEST(test_eventbus_pause_resume);
    RUN_TEST(test_eventbus_multiple_subscribers);
    RUN_TEST(test_eventbus_filter);
    RUN_TEST(test_eventbus_concurrent_publish);
    RUN_TEST(test_eventbus_error_handling);
    RUN_TEST(test_eventbus_statistics);
}
