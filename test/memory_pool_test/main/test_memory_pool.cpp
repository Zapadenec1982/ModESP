/**
 * @file test_memory_pool.cpp
 * @brief Unit tests for MemoryPool and MemoryDiagnostics
 *
 * Covers functionality from TODO-002.
 */

#include <unity.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// Припускаємо, що ваші файли знаходяться тут
#include "memory/memory_pool.h"
#include "memory/memory_diagnostics.h"
#include "memory/pooled_ptr.h"

// --- Конфігурація для тестів ---
constexpr size_t TEST_BLOCK_SIZE = 64;
constexpr size_t TEST_BLOCK_COUNT = 16;

// Створюємо глобальний пул для тестів
static MemoryPool<TEST_BLOCK_SIZE, TEST_BLOCK_COUNT> g_test_pool("TestPool");

TEST_CASE("1. MemoryPool: Basic Allocation and Deallocation", "[memory_pool]")
{
    auto stats_before = MemoryDiagnostics::get_stats(g_test_pool);
    TEST_ASSERT_EQUAL(TEST_BLOCK_COUNT, stats_before.free_blocks);

    // Allocate one block
    void* block = g_test_pool.allocate();
    TEST_ASSERT_NOT_NULL(block);

    auto stats_after_alloc = MemoryDiagnostics::get_stats(g_test_pool);
    TEST_ASSERT_EQUAL(1, stats_after_alloc.used_blocks);
    TEST_ASSERT_EQUAL(TEST_BLOCK_COUNT - 1, stats_after_alloc.free_blocks);

    // Deallocate the block
    g_test_pool.deallocate(block);

    auto stats_after_dealloc = MemoryDiagnostics::get_stats(g_test_pool);
    TEST_ASSERT_EQUAL(0, stats_after_dealloc.used_blocks);
    TEST_ASSERT_EQUAL(TEST_BLOCK_COUNT, stats_after_dealloc.free_blocks);
}

TEST_CASE("2. MemoryPool: Pool Exhaustion", "[memory_pool]")
{
    void* blocks[TEST_BLOCK_COUNT];

    // Allocate all available blocks
    for (size_t i = 0; i < TEST_BLOCK_COUNT; ++i) {
        blocks[i] = g_test_pool.allocate();
        TEST_ASSERT_NOT_NULL(blocks[i]);
    }

    // Next allocation should fail
    void* extra_block = g_test_pool.allocate();
    TEST_ASSERT_NULL(extra_block);

    auto stats = MemoryDiagnostics::get_stats(g_test_pool);
    TEST_ASSERT_EQUAL(TEST_BLOCK_COUNT, stats.used_blocks);
    TEST_ASSERT_EQUAL(0, stats.free_blocks);

    // Free one block
    g_test_pool.deallocate(blocks[0]);

    // Now allocation should succeed
    extra_block = g_test_pool.allocate();
    TEST_ASSERT_NOT_NULL(extra_block);

    // Cleanup
    g_test_pool.deallocate(extra_block);
    for (size_t i = 1; i < TEST_BLOCK_COUNT; ++i) {
        g_test_pool.deallocate(blocks[i]);
    }
}

TEST_CASE("3. PooledPtr: RAII Test", "[memory_pool]")
{
    auto stats_before = MemoryDiagnostics::get_stats(g_test_pool);
    TEST_ASSERT_EQUAL(0, stats_before.used_blocks);

    {
        // Allocate using PooledPtr
        auto ptr = make_pooled<uint8_t[TEST_BLOCK_SIZE]>(g_test_pool);
        TEST_ASSERT_NOT_NULL(ptr.get());

        auto stats_inside = MemoryDiagnostics::get_stats(g_test_pool);
        TEST_ASSERT_EQUAL(1, stats_inside.used_blocks);
    } // ptr goes out of scope here, should deallocate automatically

    auto stats_after = MemoryDiagnostics::get_stats(g_test_pool);
    TEST_ASSERT_EQUAL(0, stats_after.used_blocks);
}

TEST_CASE("4. MemoryDiagnostics: Leak Detection", "[memory_pool]")
{
    // Ensure pool is empty
    TEST_ASSERT_EQUAL(0, MemoryDiagnostics::get_stats(g_test_pool).used_blocks);

    // Simulate a leak
    void* leaked_block = g_test_pool.allocate();
    TEST_ASSERT_NOT_NULL(leaked_block);

    // Run leak detection
    size_t leaks_found = MemoryDiagnostics::detect_leaks(g_test_pool);
    TEST_ASSERT_EQUAL(1, leaks_found);

    // Cleanup
    g_test_pool.deallocate(leaked_block);

    // Verify no leaks now
    leaks_found = MemoryDiagnostics::detect_leaks(g_test_pool);
    TEST_ASSERT_EQUAL(0, leaks_found);
}

TEST_CASE("5. Performance: Pool vs Malloc", "[memory_pool]")
{
    const int iterations = 5000;
    void* blocks[TEST_BLOCK_COUNT];

    // --- Test MemoryPool speed ---
    uint64_t start_time_pool = esp_timer_get_time();
    for (int i = 0; i < iterations; ++i) {
        for(size_t j = 0; j < TEST_BLOCK_COUNT; ++j) blocks[j] = g_test_pool.allocate();
        for(size_t j = 0; j < TEST_BLOCK_COUNT; ++j) g_test_pool.deallocate(blocks[j]);
    }
    uint64_t end_time_pool = esp_timer_get_time();
    uint64_t duration_pool = end_time_pool - start_time_pool;

    // --- Test malloc/free speed ---
    uint64_t start_time_malloc = esp_timer_get_time();
    for (int i = 0; i < iterations; ++i) {
        for(size_t j = 0; j < TEST_BLOCK_COUNT; ++j) blocks[j] = malloc(TEST_BLOCK_SIZE);
        for(size_t j = 0; j < TEST_BLOCK_COUNT; ++j) free(blocks[j]);
    }
    uint64_t end_time_malloc = esp_timer_get_time();
    uint64_t duration_malloc = end_time_malloc - start_time_malloc;

    printf("Performance Test (%d iterations of %zu blocks):\n", iterations, TEST_BLOCK_COUNT);
    printf("  MemoryPool: %llu us\n", duration_pool);
    printf("  malloc/free: %llu us\n", duration_malloc);

    // MemoryPool should be significantly faster
    TEST_ASSERT_LESS_THAN(duration_malloc, duration_pool);
}


// --- Thread Safety Test ---

static SemaphoreHandle_t s_test_done_sem;

void memory_stress_task(void* pvParameters)
{
    const int iterations = 2000;
    void* blocks[TEST_BLOCK_COUNT];

    for (int i = 0; i < iterations; ++i) {
        // Allocate all blocks
        for (size_t j = 0; j < TEST_BLOCK_COUNT / 2; ++j) {
            blocks[j] = g_test_pool.allocate();
        }
        // Yield to potentially cause a race condition
        vTaskDelay(pdMS_TO_TICKS(1));
        // Deallocate all blocks
        for (size_t j = 0; j < TEST_BLOCK_COUNT / 2; ++j) {
            if (blocks[j]) {
                g_test_pool.deallocate(blocks[j]);
            }
        }
    }

    xSemaphoreGive(s_test_done_sem);
    vTaskDelete(NULL);
}

TEST_CASE("6. Thread Safety: Concurrent Access", "[memory_pool][multithread]")
{
    s_test_done_sem = xSemaphoreCreateCounting(2, 0);
    TEST_ASSERT_NOT_NULL(s_test_done_sem);

    // Ensure pool is empty before starting
    TEST_ASSERT_EQUAL(0, MemoryDiagnostics::get_stats(g_test_pool).used_blocks);

    printf("Starting thread safety test with 2 tasks...\n");

    // Create two tasks that will hammer the memory pool
    xTaskCreate(memory_stress_task, "StressTask1", 4096, NULL, 5, NULL);
    xTaskCreate(memory_stress_task, "StressTask2", 4096, NULL, 5, NULL);

    // Wait for both tasks to complete
    xSemaphoreTake(s_test_done_sem, portMAX_DELAY);
    xSemaphoreTake(s_test_done_sem, portMAX_DELAY);

    printf("Thread safety test finished.\n");

    // Check final state of the pool
    auto final_stats = MemoryDiagnostics::get_stats(g_test_pool);
    TEST_ASSERT_EQUAL(0, final_stats.used_blocks);
    TEST_ASSERT_EQUAL(TEST_BLOCK_COUNT, final_stats.free_blocks);

    // Check for leaks, which would indicate corruption
    size_t leaks_found = MemoryDiagnostics::detect_leaks(g_test_pool);
    TEST_ASSERT_EQUAL(0, leaks_found);

    vSemaphoreDelete(s_test_done_sem);
}

// Entry point for the test application
extern "C" void app_main(void)
{
    printf("Running Memory Pool tests...\n");
    // Delay to ensure the monitor is ready and can catch the output
    vTaskDelay(pdMS_TO_TICKS(2000));
    unity_run_menu();
}
