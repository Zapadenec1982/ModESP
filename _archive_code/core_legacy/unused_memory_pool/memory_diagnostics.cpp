/**
 * @file memory_diagnostics.cpp
 * @brief Memory diagnostics implementation
 */

#include "memory_diagnostics.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <inttypes.h>

namespace ModESP {
namespace Memory {

static const char* TAG = "MemDiag";

// JSON conversion for diagnostics
nlohmann::json SystemDiagnostics::to_json() const {
    nlohmann::json j;
    
    j["total_capacity"] = total_capacity;
    j["total_allocated"] = total_allocated;
    j["peak_allocated"] = peak_allocated;
    j["overall_utilization"] = overall_utilization;
    
    j["fragmentation_index"] = fragmentation_index;
    j["largest_free_block"] = largest_free_block;
    
    j["total_allocations"] = total_allocations;
    j["total_deallocations"] = total_deallocations;
    j["allocation_failures"] = allocation_failures;
    j["total_bytes_served"] = total_bytes_served;
    
    j["pools"] = nlohmann::json::array();
    for (const auto& pool : pools) {
        nlohmann::json pool_json;
        pool_json["name"] = pool.name;
        pool_json["block_size"] = pool.block_size;
        pool_json["total_blocks"] = pool.total_blocks;
        pool_json["used_blocks"] = pool.used_blocks;
        pool_json["peak_blocks"] = pool.peak_blocks;
        pool_json["allocation_rate"] = pool.allocation_rate;
        pool_json["deallocation_rate"] = pool.deallocation_rate;
        pool_json["avg_hold_time_ms"] = pool.avg_hold_time_ms;
        pool_json["utilization_percent"] = pool.utilization_percent;
        pool_json["allocation_failures"] = pool.allocation_failures;
        j["pools"].push_back(pool_json);
    }
    
    j["alerts"]["low_memory"] = low_memory_warning;
    j["alerts"]["critical_memory"] = critical_memory_alert;
    j["alerts"]["fragmentation"] = fragmentation_warning;
    
    j["uptime_seconds"] = uptime_seconds;
    j["last_reset_timestamp"] = last_reset_timestamp;
    
    return j;
}

// MemoryDiagnostics implementation
MemoryDiagnostics::MemoryDiagnostics(MemoryPoolManager& manager) 
    : pool_manager_(manager), last_pressure_level_(0) {
    
    #ifdef CONFIG_MEMORY_POOL_TRACE_ALLOCATIONS
    trace_mutex_ = xSemaphoreCreateMutex();
    traces_.reserve(MAX_TRACES);
    #endif
}

MemoryDiagnostics::~MemoryDiagnostics() {
    #ifdef CONFIG_MEMORY_POOL_TRACE_ALLOCATIONS
    if (trace_mutex_) {
        vSemaphoreDelete(trace_mutex_);
    }
    #endif
}

SystemDiagnostics MemoryDiagnostics::get_diagnostics() {
    SystemDiagnostics diag;
    
    // Get pool manager reference
    auto& pm = pool_manager_;
    
    // Overall statistics
    diag.total_capacity = pm.get_total_capacity();
    diag.overall_utilization = pm.get_overall_utilization();
    
    // Calculate total allocated
    size_t total_alloc = 0;
    
    // Tiny pool metrics
    {
        PoolMetrics metrics;
        metrics.name = "TINY";
        metrics.block_size = 32;
        metrics.total_blocks = 128;
        auto stats = pm.get_tiny_pool().get_stats();
        metrics.used_blocks = stats.allocated_count;
        metrics.peak_blocks = stats.peak_usage;
        metrics.utilization_percent = pm.get_tiny_pool().get_utilization_percent();
        metrics.allocation_failures = stats.allocation_failures;
        metrics.allocation_rate = allocation_rates_[0].rate_per_second;
        metrics.deallocation_rate = deallocation_rates_[0].rate_per_second;
        metrics.avg_hold_time_ms = stats.average_hold_time_ms;
        
        total_alloc += metrics.used_blocks * metrics.block_size;
        diag.pools.push_back(metrics);
    }
    
    // Small pool metrics
    {
        PoolMetrics metrics;
        metrics.name = "SMALL";
        metrics.block_size = 64;
        metrics.total_blocks = 64;
        auto stats = pm.get_small_pool().get_stats();
        metrics.used_blocks = stats.allocated_count;
        metrics.peak_blocks = stats.peak_usage;
        metrics.utilization_percent = pm.get_small_pool().get_utilization_percent();
        metrics.allocation_failures = stats.allocation_failures;
        metrics.allocation_rate = allocation_rates_[1].rate_per_second;
        metrics.deallocation_rate = deallocation_rates_[1].rate_per_second;
        metrics.avg_hold_time_ms = stats.average_hold_time_ms;
        
        total_alloc += metrics.used_blocks * metrics.block_size;
        diag.pools.push_back(metrics);
    }
    
    // Medium pool metrics
    {
        PoolMetrics metrics;
        metrics.name = "MEDIUM";
        metrics.block_size = 128;
        metrics.total_blocks = 32;
        auto stats = pm.get_medium_pool().get_stats();
        metrics.used_blocks = stats.allocated_count;
        metrics.peak_blocks = stats.peak_usage;
        metrics.utilization_percent = pm.get_medium_pool().get_utilization_percent();
        metrics.allocation_failures = stats.allocation_failures;
        metrics.allocation_rate = allocation_rates_[2].rate_per_second;
        metrics.deallocation_rate = deallocation_rates_[2].rate_per_second;
        metrics.avg_hold_time_ms = stats.average_hold_time_ms;
        
        total_alloc += metrics.used_blocks * metrics.block_size;
        diag.pools.push_back(metrics);
    }
    
    // Large pool metrics
    {
        PoolMetrics metrics;
        metrics.name = "LARGE";
        metrics.block_size = 256;
        metrics.total_blocks = 16;
        auto stats = pm.get_large_pool().get_stats();
        metrics.used_blocks = stats.allocated_count;
        metrics.peak_blocks = stats.peak_usage;
        metrics.utilization_percent = pm.get_large_pool().get_utilization_percent();
        metrics.allocation_failures = stats.allocation_failures;
        metrics.allocation_rate = allocation_rates_[3].rate_per_second;
        metrics.deallocation_rate = deallocation_rates_[3].rate_per_second;
        metrics.avg_hold_time_ms = stats.average_hold_time_ms;
        
        total_alloc += metrics.used_blocks * metrics.block_size;
        diag.pools.push_back(metrics);
    }
    
    // XLarge pool metrics
    {
        PoolMetrics metrics;
        metrics.name = "XLARGE";
        metrics.block_size = 512;
        metrics.total_blocks = 8;
        auto stats = pm.get_xlarge_pool().get_stats();
        metrics.used_blocks = stats.allocated_count;
        metrics.peak_blocks = stats.peak_usage;
        metrics.utilization_percent = pm.get_xlarge_pool().get_utilization_percent();
        metrics.allocation_failures = stats.allocation_failures;
        metrics.allocation_rate = allocation_rates_[4].rate_per_second;
        metrics.deallocation_rate = deallocation_rates_[4].rate_per_second;
        metrics.avg_hold_time_ms = stats.average_hold_time_ms;
        
        total_alloc += metrics.used_blocks * metrics.block_size;
        diag.pools.push_back(metrics);
    }
    
    diag.total_allocated = total_alloc;
    
    // Calculate fragmentation
    diag.fragmentation_index = calculate_fragmentation_index();
    diag.largest_free_block = find_largest_free_block();
    
    // Alerts
    diag.low_memory_warning = pm.has_low_memory_alert();
    diag.critical_memory_alert = pm.has_critical_memory_alert();
    diag.fragmentation_warning = (diag.fragmentation_index > 50);
    
    // Timing
    diag.uptime_seconds = esp_timer_get_time() / 1000000;
    
    return diag;
}

uint8_t MemoryDiagnostics::calculate_fragmentation_index() {
    // Simple fragmentation metric based on free block distribution
    // 0 = no fragmentation (all free blocks in largest pool)
    // 100 = severe fragmentation (only tiny blocks available)
    
    auto& pm = pool_manager_;
    
    uint32_t weighted_free = 0;
    uint32_t total_free = 0;
    
    // Weight larger blocks more heavily
    weighted_free += pm.get_xlarge_pool().get_free_blocks() * 5;
    weighted_free += pm.get_large_pool().get_free_blocks() * 4;
    weighted_free += pm.get_medium_pool().get_free_blocks() * 3;
    weighted_free += pm.get_small_pool().get_free_blocks() * 2;
    weighted_free += pm.get_tiny_pool().get_free_blocks() * 1;
    
    total_free += pm.get_xlarge_pool().get_free_blocks();
    total_free += pm.get_large_pool().get_free_blocks();
    total_free += pm.get_medium_pool().get_free_blocks();
    total_free += pm.get_small_pool().get_free_blocks();
    total_free += pm.get_tiny_pool().get_free_blocks();
    
    if (total_free == 0) return 100; // Completely fragmented
    
    // Calculate optimal weight if all free blocks were xlarge
    uint32_t optimal_weight = total_free * 5;
    
    // Fragmentation = how far we are from optimal
    return 100 - ((weighted_free * 100) / optimal_weight);
}

size_t MemoryDiagnostics::find_largest_free_block() {
    auto& pm = pool_manager_;
    
    if (!pm.get_xlarge_pool().is_exhausted()) return 512;
    if (!pm.get_large_pool().is_exhausted()) return 256;
    if (!pm.get_medium_pool().is_exhausted()) return 128;
    if (!pm.get_small_pool().is_exhausted()) return 64;
    if (!pm.get_tiny_pool().is_exhausted()) return 32;
    
    return 0;
}

std::string MemoryDiagnostics::generate_report() {
    auto diag = get_diagnostics();
    std::stringstream ss;
    
    ss << "\n=== Memory Pool Diagnostics Report ===\n";
    ss << "Uptime: " << diag.uptime_seconds << " seconds\n";
    ss << "Overall Utilization: " << static_cast<int>(diag.overall_utilization) << "%\n";
    ss << "Total Capacity: " << diag.total_capacity << " bytes\n";
    ss << "Currently Allocated: " << diag.total_allocated << " bytes\n";
    ss << "Peak Allocated: " << diag.peak_allocated << " bytes\n";
    ss << "Fragmentation Index: " << static_cast<int>(diag.fragmentation_index) << "%\n";
    ss << "Largest Free Block: " << diag.largest_free_block << " bytes\n";
    
    ss << "\n--- Pool Statistics ---\n";
    for (const auto& pool : diag.pools) {
        ss << std::setw(6) << pool.name << " Pool: "
           << pool.used_blocks << "/" << pool.total_blocks << " blocks ("
           << static_cast<int>(pool.utilization_percent) << "%), "
           << "Failures: " << pool.allocation_failures << ", "
           << "Rate: " << pool.allocation_rate << " alloc/s\n";
    }
    
    if (diag.critical_memory_alert) {
        ss << "\n!!! CRITICAL MEMORY ALERT !!!\n";
    } else if (diag.low_memory_warning) {
        ss << "\n! Low Memory Warning !\n";
    }
    
    return ss.str();
}

void MemoryDiagnostics::log_stats(const char* tag) {
    ESP_LOGI(tag, "%s", generate_report().c_str());
}

// Benchmark implementation
nlohmann::json MemoryPoolBenchmark::BenchmarkResult::to_json() const {
    nlohmann::json j;
    j["test_name"] = test_name;
    j["iterations"] = iterations;
    j["total_time_us"] = total_time_us;
    j["avg_time_ns"] = avg_time_ns;
    j["min_time_ns"] = min_time_ns;
    j["max_time_ns"] = max_time_ns;
    j["failures"] = failures;
    return j;
}

MemoryPoolBenchmark::BenchmarkResult MemoryPoolBenchmark::benchmark_allocation(
    size_t size, uint32_t iterations) {
    
    BenchmarkResult result;
    result.test_name = "Allocation/Deallocation";
    result.iterations = iterations;
    result.failures = 0;
    result.min_time_ns = UINT64_MAX;
    result.max_time_ns = 0;
    
    auto& pm = Memory::get_pool_manager();
    std::vector<void*> allocations;
    allocations.reserve(iterations);
    
    uint64_t start = esp_timer_get_time();
    
    // Allocation phase
    for (uint32_t i = 0; i < iterations; ++i) {
        uint64_t alloc_start = esp_timer_get_time();
        void* ptr = pm.allocate(size);
        uint64_t alloc_time = (esp_timer_get_time() - alloc_start) * 1000; // to ns
        
        if (ptr) {
            allocations.push_back(ptr);
            result.min_time_ns = std::min(result.min_time_ns, alloc_time);
            result.max_time_ns = std::max(result.max_time_ns, alloc_time);
        } else {
            result.failures++;
        }
    }
    
    // Deallocation phase
    for (void* ptr : allocations) {
        pm.deallocate(ptr, size);
    }
    
    result.total_time_us = esp_timer_get_time() - start;
    result.avg_time_ns = (result.total_time_us * 1000) / (iterations * 2); // alloc + dealloc
    
    return result;
}

MemoryPoolBenchmark::BenchmarkResult MemoryPoolBenchmark::benchmark_fragmentation(
    uint32_t duration_ms) {
    
    BenchmarkResult result;
    result.test_name = "Fragmentation Test";
    result.iterations = 0;
    result.failures = 0;
    
    auto& pm = Memory::get_pool_manager();
    std::vector<std::pair<void*, size_t>> allocations;
    
    uint32_t start = xTaskGetTickCount();
    uint32_t end = start + pdMS_TO_TICKS(duration_ms);
    
    // Random allocation pattern to induce fragmentation
    const size_t sizes[] = {32, 64, 128, 256, 512};
    size_t size_idx = 0;
    
    while (xTaskGetTickCount() < end) {
        // Allocate
        size_t size = sizes[size_idx % 5];
        void* ptr = pm.allocate(size);
        
        if (ptr) {
            allocations.push_back({ptr, size});
        } else {
            result.failures++;
        }
        
        // Randomly deallocate ~50% to create fragmentation
        if (!allocations.empty() && (rand() % 100) < 50) {
            size_t idx = rand() % allocations.size();
            pm.deallocate(allocations[idx].first, allocations[idx].second);
            allocations.erase(allocations.begin() + idx);
        }
        
        result.iterations++;
        size_idx++;
        
        vTaskDelay(1); // Yield
    }
    
    // Cleanup
    for (const auto& alloc : allocations) {
        pm.deallocate(alloc.first, alloc.second);
    }
    
    result.total_time_us = duration_ms * 1000;
    
    return result;
}

// Thread function for multithreaded test
static void benchmark_thread_func(void* param) {
    auto* result = static_cast<MemoryPoolBenchmark::BenchmarkResult*>(param);
    auto& pm = Memory::get_pool_manager();
    
    const size_t sizes[] = {32, 64, 128};
    std::vector<std::pair<void*, size_t>> local_allocs;
    
    for (uint32_t i = 0; i < 1000; ++i) {
        size_t size = sizes[i % 3];
        void* ptr = pm.allocate(size);
        
        if (ptr) {
            local_allocs.push_back({ptr, size});
        } else {
            result->failures++;
        }
        
        // Deallocate some
        if (local_allocs.size() > 10) {
            auto& alloc = local_allocs.front();
            pm.deallocate(alloc.first, alloc.second);
            local_allocs.erase(local_allocs.begin());
        }
        
        result->iterations++;
    }
    
    // Cleanup
    for (const auto& alloc : local_allocs) {
        pm.deallocate(alloc.first, alloc.second);
    }
    
    vTaskDelete(NULL);
}

MemoryPoolBenchmark::BenchmarkResult MemoryPoolBenchmark::benchmark_multithreaded(
    uint32_t num_threads, uint32_t duration_ms) {
    
    BenchmarkResult result;
    result.test_name = "Multithreaded Contention";
    result.iterations = 0;
    result.failures = 0;
    
    std::vector<TaskHandle_t> tasks;
    
    uint64_t start = esp_timer_get_time();
    
    // Create threads
    for (uint32_t i = 0; i < num_threads; ++i) {
        TaskHandle_t task;
        xTaskCreate(benchmark_thread_func, "bench_thread", 
                   4096, &result, tskIDLE_PRIORITY + 1, &task);
        tasks.push_back(task);
    }
    
    // Wait for duration
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    
    // Note: threads will auto-delete
    
    result.total_time_us = esp_timer_get_time() - start;
    result.avg_time_ns = (result.total_time_us * 1000) / result.iterations;
    
    return result;
}

std::vector<MemoryPoolBenchmark::BenchmarkResult> 
MemoryPoolBenchmark::run_all_benchmarks() {
    std::vector<BenchmarkResult> results;
    
    ESP_LOGI(TAG, "Starting memory pool benchmarks...");
    
    // Test different allocation sizes
    results.push_back(benchmark_allocation(32, 10000));
    results.push_back(benchmark_allocation(128, 5000));
    results.push_back(benchmark_allocation(512, 1000));
    
    // Fragmentation test
    results.push_back(benchmark_fragmentation(5000));
    
    // Multithreaded test
    results.push_back(benchmark_multithreaded(4, 3000));
    
    ESP_LOGI(TAG, "Benchmarks complete");
    
    return results;
}

// MemoryUsageTracker implementation
MemoryUsageTracker::MemoryUsageTracker(const char* operation) 
    : operation_(operation) {
    auto& pm = Memory::get_pool_manager();
    initial_allocated_ = pm.get_overall_utilization();
    start_time_ = xTaskGetTickCount();
}

MemoryUsageTracker::~MemoryUsageTracker() {
    auto& pm = Memory::get_pool_manager();
    size_t final_allocated = pm.get_overall_utilization();
    uint32_t elapsed = xTaskGetTickCount() - start_time_;
    
    int32_t delta = static_cast<int32_t>(final_allocated) - static_cast<int32_t>(initial_allocated_);
    
    ESP_LOGI(TAG, "[%s] Memory: %" PRIu32 "%% -> %" PRIu32 "%% (delta: %+" PRId32 "%%), Time: %" PRIu32 " ms",
             operation_, 
             static_cast<uint32_t>(initial_allocated_),
             static_cast<uint32_t>(final_allocated),
             delta,
             elapsed * portTICK_PERIOD_MS);
}

} // namespace Memory
} // namespace ModESP
