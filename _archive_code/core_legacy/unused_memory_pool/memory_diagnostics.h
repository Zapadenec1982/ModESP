/**
 * @file memory_diagnostics.h
 * @brief Memory pool diagnostics and monitoring system
 * 
 * Provides comprehensive diagnostics, statistics, and monitoring
 * capabilities for the memory pool system.
 */

#ifndef MEMORY_DIAGNOSTICS_H
#define MEMORY_DIAGNOSTICS_H

#include "memory_pool.h"
#include "nlohmann/json.hpp"
#include <vector>
#include <chrono>

namespace ModESP {
namespace Memory {

/**
 * @brief Detailed pool metrics
 */
struct PoolMetrics {
    const char* name;              ///< Pool name
    size_t block_size;             ///< Size of each block
    uint16_t total_blocks;         ///< Total number of blocks
    uint16_t used_blocks;          ///< Currently allocated blocks
    uint16_t peak_blocks;          ///< Peak usage
    uint32_t allocation_rate;      ///< Allocations per second
    uint32_t deallocation_rate;    ///< Deallocations per second
    uint32_t avg_hold_time_ms;     ///< Average block hold time
    uint8_t utilization_percent;   ///< Current utilization %
    uint32_t allocation_failures;  ///< Failed allocations
};

/**
 * @brief System-wide memory diagnostics
 */
struct SystemDiagnostics {
    // Overall statistics
    size_t total_capacity;         ///< Total memory pool capacity
    size_t total_allocated;        ///< Currently allocated bytes
    size_t peak_allocated;         ///< Peak allocated bytes
    uint8_t overall_utilization;   ///< System-wide utilization %
    
    // Fragmentation analysis
    uint8_t fragmentation_index;   ///< 0-100% fragmentation metric
    size_t largest_free_block;     ///< Largest allocatable size
    
    // Performance metrics
    uint32_t total_allocations;    ///< Total allocation count
    uint32_t total_deallocations;  ///< Total deallocation count
    uint32_t allocation_failures;  ///< Total failed allocations
    uint64_t total_bytes_served;   ///< Total bytes allocated lifetime
    
    // Per-pool metrics
    std::vector<PoolMetrics> pools;
    
    // System alerts
    bool low_memory_warning;       ///< < 20% free
    bool critical_memory_alert;    ///< < 5% free
    bool fragmentation_warning;    ///< High fragmentation detected
    
    // Timing
    uint64_t uptime_seconds;       ///< System uptime
    uint64_t last_reset_timestamp; ///< Last stats reset
    
    /**
     * @brief Convert to JSON for API/logging
     */
    nlohmann::json to_json() const;
};

/**
 * @brief Memory allocation trace entry
 */
struct AllocationTrace {
    void* address;                 ///< Allocated address
    size_t size;                   ///< Allocation size
    uint32_t timestamp;            ///< Allocation time
    const char* module;            ///< Allocating module
    uint32_t line;                 ///< Source line number
    bool is_freed;                 ///< Whether freed
    uint32_t hold_time_ms;         ///< Time held (if freed)
};

/**
 * @brief Memory diagnostics manager
 */
class MemoryDiagnostics {
private:
    MemoryPoolManager& pool_manager_;
    
    // Sampling intervals
    static constexpr uint32_t RATE_SAMPLE_INTERVAL_MS = 1000;
    
    // Rate calculation
    struct RateCounter {
        uint32_t count = 0;
        uint32_t last_sample_time = 0;
        uint32_t rate_per_second = 0;
        
        void update() {
            uint32_t now = xTaskGetTickCount();
            uint32_t elapsed = now - last_sample_time;
            
            if (elapsed >= pdMS_TO_TICKS(RATE_SAMPLE_INTERVAL_MS)) {
                rate_per_second = (count * 1000) / (elapsed * portTICK_PERIOD_MS);
                count = 0;
                last_sample_time = now;
            }
        }
    };
    
    // Per-pool rate counters
    RateCounter allocation_rates_[5];
    RateCounter deallocation_rates_[5];
    
    // Allocation tracing (debug mode)
    #ifdef CONFIG_MEMORY_POOL_TRACE_ALLOCATIONS
    static constexpr size_t MAX_TRACES = 1000;
    std::vector<AllocationTrace> traces_;
    SemaphoreHandle_t trace_mutex_;
    #endif

public:
    /**
     * @brief Initialize diagnostics
     */
    explicit MemoryDiagnostics(MemoryPoolManager& manager);
    ~MemoryDiagnostics();
    
    /**
     * @brief Get current system diagnostics
     */
    SystemDiagnostics get_diagnostics();
    
    /**
     * @brief Calculate fragmentation index
     * 
     * @return 0-100 where 0 is no fragmentation
     */
    uint8_t calculate_fragmentation_index();
    
    /**
     * @brief Find largest allocatable block size
     */
    size_t find_largest_free_block();
    
    /**
     * @brief Update allocation/deallocation rates
     */
    void update_rates();
    
    /**
     * @brief Trace allocation (debug mode)
     */
    #ifdef CONFIG_MEMORY_POOL_TRACE_ALLOCATIONS
    void trace_allocation(void* ptr, size_t size, const char* module, uint32_t line);
    void trace_deallocation(void* ptr);
    std::vector<AllocationTrace> get_active_allocations();
    void clear_traces();
    #endif
    
    /**
     * @brief Generate memory report
     */
    std::string generate_report();
    
    /**
     * @brief Log memory statistics
     */
    void log_stats(const char* tag = "MemDiag");
    
    /**
     * @brief Check for memory leaks
     * 
     * @return Vector of potential leaks (allocations > threshold age)
     */
    std::vector<AllocationTrace> detect_potential_leaks(uint32_t threshold_ms = 60000);
    
    /**
     * @brief Register allocation hook
     */
    using AllocationHook = std::function<void(size_t size, bool success)>;
    void register_allocation_hook(AllocationHook hook);
    
    /**
     * @brief Memory pressure callback
     */
    using MemoryPressureCallback = std::function<void(uint8_t utilization)>;
    void register_pressure_callback(MemoryPressureCallback callback);
    
private:
    std::vector<AllocationHook> allocation_hooks_;
    std::vector<MemoryPressureCallback> pressure_callbacks_;
    uint8_t last_pressure_level_ = 0;
    
    /**
     * @brief Notify pressure callbacks if threshold crossed
     */
    void check_memory_pressure(uint8_t current_utilization);
};

/**
 * @brief Memory pool benchmark utilities
 */
class MemoryPoolBenchmark {
public:
    struct BenchmarkResult {
        const char* test_name;
        uint32_t iterations;
        uint64_t total_time_us;
        uint64_t avg_time_ns;
        uint64_t min_time_ns;
        uint64_t max_time_ns;
        uint32_t failures;
        
        nlohmann::json to_json() const;
    };
    
    /**
     * @brief Run allocation/deallocation benchmark
     */
    static BenchmarkResult benchmark_allocation(size_t size, uint32_t iterations);
    
    /**
     * @brief Run fragmentation test
     */
    static BenchmarkResult benchmark_fragmentation(uint32_t duration_ms);
    
    /**
     * @brief Run thread contention test
     */
    static BenchmarkResult benchmark_multithreaded(uint32_t num_threads, 
                                                   uint32_t duration_ms);
    
    /**
     * @brief Run stress test
     */
    static BenchmarkResult benchmark_stress_test(uint32_t duration_ms);
    
    /**
     * @brief Run all benchmarks
     */
    static std::vector<BenchmarkResult> run_all_benchmarks();
    
    /**
     * @brief Generate benchmark report
     */
    static std::string generate_benchmark_report(const std::vector<BenchmarkResult>& results);
};

/**
 * @brief RAII memory usage tracker
 */
class MemoryUsageTracker {
private:
    const char* operation_;
    size_t initial_allocated_;
    uint32_t start_time_;
    
public:
    explicit MemoryUsageTracker(const char* operation);
    ~MemoryUsageTracker();
};

/**
 * @brief Macro for easy memory tracking
 */
#define TRACK_MEMORY_USAGE(operation) \
    ModESP::Memory::MemoryUsageTracker _tracker(operation)

} // namespace Memory
} // namespace ModESP

#endif // MEMORY_DIAGNOSTICS_H
