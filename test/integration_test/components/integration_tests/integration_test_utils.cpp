/**
 * @file integration_test_utils.cpp
 * @brief Utility functions for integration tests
 */

#include "integration_test_common.h"
#include "application.h"
#include "event_bus.h"

static const char* TAG = "IntegrationTestUtils";

void integration_test_start_metrics(integration_test_metrics_t* metrics) {
    if (!metrics) return;
    
    metrics->start_time_ms = esp_timer_get_time() / 1000;
    metrics->start_free_heap = esp_get_free_heap_size();
    metrics->min_free_heap = metrics->start_free_heap;
    metrics->max_cycle_time_us = 0;
    metrics->max_cpu_usage = 0;
    metrics->system_stable = true;
    
    ESP_LOGI(TAG, "Started metrics collection at %lu ms, heap: %zu bytes", 
             metrics->start_time_ms, metrics->start_free_heap);
}

void integration_test_stop_metrics(integration_test_metrics_t* metrics) {
    if (!metrics) return;
    
    metrics->end_time_ms = esp_timer_get_time() / 1000;
    metrics->end_free_heap = esp_get_free_heap_size();
    
    // Update min heap if current is lower
    if (metrics->end_free_heap < metrics->min_free_heap) {
        metrics->min_free_heap = metrics->end_free_heap;
    }
    
    ESP_LOGI(TAG, "Stopped metrics collection at %lu ms, heap: %zu bytes", 
             metrics->end_time_ms, metrics->end_free_heap);
}

void integration_test_print_metrics(const char* test_name, const integration_test_metrics_t* metrics) {
    if (!metrics || !test_name) return;
    
    uint32_t duration_ms = metrics->end_time_ms - metrics->start_time_ms;
    int32_t heap_delta = (int32_t)metrics->end_free_heap - (int32_t)metrics->start_free_heap;
    
    ESP_LOGI(TAG, "=== Metrics for %s ===", test_name);
    ESP_LOGI(TAG, "Duration: %lu ms", duration_ms);
    ESP_LOGI(TAG, "Heap: start=%zu, end=%zu, min=%zu, delta=%ld", 
             metrics->start_free_heap, metrics->end_free_heap, 
             metrics->min_free_heap, heap_delta);
    ESP_LOGI(TAG, "Max cycle time: %lu μs", metrics->max_cycle_time_us);
    ESP_LOGI(TAG, "Max CPU usage: %u%%", metrics->max_cpu_usage);
    ESP_LOGI(TAG, "System stable: %s", metrics->system_stable ? "YES" : "NO");
    ESP_LOGI(TAG, "===========================");
}

bool integration_test_verify_stability(uint32_t duration_ms) {
    ESP_LOGI(TAG, "Verifying system stability for %lu ms...", duration_ms);
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    uint32_t last_check = start_time;
    uint32_t check_count = 0;
    uint32_t stability_failures = 0;
    
    size_t initial_heap = esp_get_free_heap_size();
    
    while ((esp_timer_get_time() / 1000) - start_time < duration_ms) {
        uint32_t current_time = esp_timer_get_time() / 1000;
        
        // Check every 500ms
        if (current_time - last_check >= 500) {
            check_count++;
            
            // Check application state
            if (!Application::is_running()) {
                ESP_LOGW(TAG, "Stability check %lu: Application not running", check_count);
                stability_failures++;
            }
            
            // Check health
            if (!Application::check_health()) {
                ESP_LOGW(TAG, "Stability check %lu: Health check failed", check_count);
                stability_failures++;
            }
            
            // Check memory
            size_t current_heap = esp_get_free_heap_size();
            if (current_heap < CRITICAL_FREE_HEAP_BYTES) {
                ESP_LOGW(TAG, "Stability check %lu: Low memory (%zu bytes)", 
                         check_count, current_heap);
                stability_failures++;
            }
            
            // Check for excessive memory loss
            if (current_heap < initial_heap - 50000) {
                ESP_LOGW(TAG, "Stability check %lu: Excessive memory loss (%zu -> %zu)", 
                         check_count, initial_heap, current_heap);
                stability_failures++;
            }
            
            last_check = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    float stability_rate = (float)(check_count - stability_failures) / check_count;
    
    ESP_LOGI(TAG, "Stability verification: %lu/%lu checks passed (%.1f%%)", 
             check_count - stability_failures, check_count, stability_rate * 100);
    
    return stability_rate >= 0.95f; // 95% stability required
}

void integration_test_generate_load(uint32_t duration_ms, uint8_t intensity) {
    ESP_LOGI(TAG, "Generating system load for %lu ms at intensity %u/10...", 
             duration_ms, intensity);
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    uint32_t events_generated = 0;
    uint32_t allocations_made = 0;
    
    // Adjust load parameters based on intensity
    uint32_t event_interval_ms = 100 / (intensity + 1);  // More events = higher intensity
    uint32_t alloc_size = 1024 * intensity;              // Larger allocations = higher intensity
    uint32_t cpu_work = 1000 * intensity;                // More CPU work = higher intensity
    
    std::vector<void*> temp_allocations;
    
    while ((esp_timer_get_time() / 1000) - start_time < duration_ms) {
        // Generate events
        if (events_generated % (event_interval_ms + 1) == 0) {
            EventBus::publish("load.test.event", {
                {"intensity", intensity},
                {"event_number", events_generated},
                {"timestamp", esp_timer_get_time()}
            });
            events_generated++;
        }
        
        // Memory allocation/deallocation
        if (intensity > 3 && allocations_made % 10 == 0) {
            void* ptr = malloc(alloc_size);
            if (ptr) {
                temp_allocations.push_back(ptr);
                allocations_made++;
                
                // Free some allocations to prevent memory exhaustion
                if (temp_allocations.size() > 20) {
                    free(temp_allocations.front());
                    temp_allocations.erase(temp_allocations.begin());
                }
            }
        }
        
        // CPU intensive work
        if (intensity > 5) {
            volatile float result = 0;
            for (uint32_t i = 0; i < cpu_work; i++) {
                result += sqrt(i * 3.14159f);
            }
        }
        
        // Small delay to prevent watchdog
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Cleanup allocations
    for (void* ptr : temp_allocations) {
        free(ptr);
    }
    
    ESP_LOGI(TAG, "Load generation complete: %lu events, %lu allocations", 
             events_generated, allocations_made);
}

bool integration_test_wait_for_running(uint32_t timeout_ms) {
    ESP_LOGI(TAG, "Waiting for application to reach RUNNING state (timeout: %lu ms)...", 
             timeout_ms);
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    while ((esp_timer_get_time() / 1000) - start_time < timeout_ms) {
        if (Application::is_running() && 
            Application::get_state() == Application::State::RUNNING) {
            ESP_LOGI(TAG, "Application reached RUNNING state");
            return true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGW(TAG, "Timeout waiting for application RUNNING state");
    return false;
} 