/**
 * @file memory_pool.cpp
 * @brief Memory pool implementation for industrial embedded systems
 */

#include "memory_pool.h"
#include "esp_log.h"
#include "esp_system.h"

// Default configuration values if not defined in Kconfig
#ifndef CONFIG_MEMORY_POOL_TINY_COUNT
#define CONFIG_MEMORY_POOL_TINY_COUNT 128
#endif

#ifndef CONFIG_MEMORY_POOL_SMALL_COUNT
#define CONFIG_MEMORY_POOL_SMALL_COUNT 64
#endif

#ifndef CONFIG_MEMORY_POOL_MEDIUM_COUNT
#define CONFIG_MEMORY_POOL_MEDIUM_COUNT 32
#endif

#ifndef CONFIG_MEMORY_POOL_LARGE_COUNT
#define CONFIG_MEMORY_POOL_LARGE_COUNT 16
#endif

#ifndef CONFIG_MEMORY_POOL_XLARGE_COUNT
#define CONFIG_MEMORY_POOL_XLARGE_COUNT 8
#endif

static const char* TAG = "MemoryPool";

namespace MemoryPool {

// Static instance
MemoryPoolManager* MemoryPoolManager::instance_ = nullptr;

esp_err_t MemoryPoolManager::init(AllocationStrategy strategy) {
    default_strategy_ = strategy;
    
    ESP_LOGI(TAG, "Initializing Memory Pool System");
    ESP_LOGI(TAG, "Total memory: %zu bytes", 
             tiny_pool_.get_total_blocks() * tiny_pool_.get_block_size() +
             small_pool_.get_total_blocks() * small_pool_.get_block_size() +
             medium_pool_.get_total_blocks() * medium_pool_.get_block_size() +
             large_pool_.get_total_blocks() * large_pool_.get_block_size() +
             xlarge_pool_.get_total_blocks() * xlarge_pool_.get_block_size());
    
    ESP_LOGI(TAG, "Pool configuration:");
    ESP_LOGI(TAG, "  TINY   (32B):  %d blocks", CONFIG_MEMORY_POOL_TINY_COUNT);
    ESP_LOGI(TAG, "  SMALL  (64B):  %d blocks", CONFIG_MEMORY_POOL_SMALL_COUNT);
    ESP_LOGI(TAG, "  MEDIUM (128B): %d blocks", CONFIG_MEMORY_POOL_MEDIUM_COUNT);
    ESP_LOGI(TAG, "  LARGE  (256B): %d blocks", CONFIG_MEMORY_POOL_LARGE_COUNT);
    ESP_LOGI(TAG, "  XLARGE (512B): %d blocks", CONFIG_MEMORY_POOL_XLARGE_COUNT);
    
    return ESP_OK;
}

void* MemoryPoolManager::allocate(size_t size, AllocationStrategy strategy) {
    if (size == 0 || size > static_cast<size_t>(BlockSize::XLARGE)) {
        ESP_LOGW(TAG, "Invalid allocation size: %zu", size);
        return nullptr;
    }
    
    // Select appropriate pool
    BlockSize pool_size = select_pool_size(size);
    void* ptr = nullptr;
    
    // Try to allocate from selected pool
    switch (pool_size) {
        case BlockSize::TINY:
            ptr = tiny_pool_.allocate();
            break;
        case BlockSize::SMALL:
            ptr = small_pool_.allocate();
            break;
        case BlockSize::MEDIUM:
            ptr = medium_pool_.allocate();
            break;
        case BlockSize::LARGE:
            ptr = large_pool_.allocate();
            break;
        case BlockSize::XLARGE:
            ptr = xlarge_pool_.allocate();
            break;
    }
    
    // Handle allocation failure based on strategy
    if (!ptr) {
        switch (strategy) {
            case AllocationStrategy::STRICT_NO_FALLBACK:
                ESP_LOGE(TAG, "Memory pool exhausted for size %zu", size);
                if (is_low_memory()) {
                    ESP_LOGE(TAG, "CRITICAL: System low on memory!");
                }
                break;
                
            case AllocationStrategy::FALLBACK_TO_HEAP:
                #ifdef CONFIG_MEMORY_POOL_ALLOW_HEAP_FALLBACK
                ESP_LOGW(TAG, "Falling back to heap for size %zu", size);
                ptr = malloc(size);
                #else
                ESP_LOGE(TAG, "Heap fallback disabled in configuration");
                #endif
                break;
                
            case AllocationStrategy::WAIT_WITH_TIMEOUT:
                // TODO: Implement wait mechanism
                ESP_LOGW(TAG, "Wait strategy not yet implemented");
                break;
        }
    }
    
    return ptr;
}

esp_err_t MemoryPoolManager::deallocate(void* ptr, size_t size) {
    if (!ptr) return ESP_OK;
    
    BlockSize pool_size = select_pool_size(size);
    
    switch (pool_size) {
        case BlockSize::TINY:
            return tiny_pool_.deallocate(ptr);
        case BlockSize::SMALL:
            return small_pool_.deallocate(ptr);
        case BlockSize::MEDIUM:
            return medium_pool_.deallocate(ptr);
        case BlockSize::LARGE:
            return large_pool_.deallocate(ptr);
        case BlockSize::XLARGE:
            return xlarge_pool_.deallocate(ptr);
    }
    
    return ESP_ERR_INVALID_ARG;
}

BlockSize MemoryPoolManager::select_pool_size(size_t requested_size) const {
    if (requested_size <= 32) return BlockSize::TINY;
    if (requested_size <= 64) return BlockSize::SMALL;
    if (requested_size <= 128) return BlockSize::MEDIUM;
    if (requested_size <= 256) return BlockSize::LARGE;
    return BlockSize::XLARGE;
}

PoolDiagnostics MemoryPoolManager::get_diagnostics() const {
    PoolDiagnostics diag{};
    
    // Calculate total usage
    uint32_t total_allocated = 
        tiny_pool_.get_allocated_count() +
        small_pool_.get_allocated_count() +
        medium_pool_.get_allocated_count() +
        large_pool_.get_allocated_count() +
        xlarge_pool_.get_allocated_count();
        
    uint32_t total_blocks = 
        tiny_pool_.get_total_blocks() +
        small_pool_.get_total_blocks() +
        medium_pool_.get_total_blocks() +
        large_pool_.get_total_blocks() +
        xlarge_pool_.get_total_blocks();
    
    diag.current_usage = total_allocated;
    diag.peak_usage = 
        tiny_pool_.get_peak_usage() +
        small_pool_.get_peak_usage() +
        medium_pool_.get_peak_usage() +
        large_pool_.get_peak_usage() +
        xlarge_pool_.get_peak_usage();
    
    diag.allocation_failures = 
        tiny_pool_.get_allocation_failures() +
        small_pool_.get_allocation_failures() +
        medium_pool_.get_allocation_failures() +
        large_pool_.get_allocation_failures() +
        xlarge_pool_.get_allocation_failures();
    
    // Calculate fragmentation (simplified)
    diag.fragmentation_index = (total_allocated * 100) / total_blocks;
    
    // Fill pool metrics
    diag.pools[0] = {
        "TINY", 32, 
        tiny_pool_.get_total_blocks(),
        tiny_pool_.get_allocated_count(),
        0, 0,  // TODO: Calculate rates
        tiny_pool_.get_peak_usage()
    };
    
    diag.pools[1] = {
        "SMALL", 64,
        small_pool_.get_total_blocks(),
        small_pool_.get_allocated_count(),
        0, 0,
        small_pool_.get_peak_usage()
    };
    
    diag.pools[2] = {
        "MEDIUM", 128,
        medium_pool_.get_total_blocks(),
        medium_pool_.get_allocated_count(),
        0, 0,
        medium_pool_.get_peak_usage()
    };
    
    diag.pools[3] = {
        "LARGE", 256,
        large_pool_.get_total_blocks(),
        large_pool_.get_allocated_count(),
        0, 0,
        large_pool_.get_peak_usage()
    };
    
    diag.pools[4] = {
        "XLARGE", 512,
        xlarge_pool_.get_total_blocks(),
        xlarge_pool_.get_allocated_count(),
        0, 0,
        xlarge_pool_.get_peak_usage()
    };
    
    // Check alerts
    uint8_t free_percent = get_free_percentage();
    diag.low_memory_warning = (free_percent < (100 - low_memory_threshold_));
    diag.critical_memory_alert = (free_percent < (100 - critical_memory_threshold_));
    
    return diag;
}

bool MemoryPoolManager::is_low_memory() const {
    return get_free_percentage() < 20;
}

uint8_t MemoryPoolManager::get_free_percentage() const {
    uint32_t total_allocated = 
        tiny_pool_.get_allocated_count() +
        small_pool_.get_allocated_count() +
        medium_pool_.get_allocated_count() +
        large_pool_.get_allocated_count() +
        xlarge_pool_.get_allocated_count();
        
    uint32_t total_blocks = 
        tiny_pool_.get_total_blocks() +
        small_pool_.get_total_blocks() +
        medium_pool_.get_total_blocks() +
        large_pool_.get_total_blocks() +
        xlarge_pool_.get_total_blocks();
    
    if (total_blocks == 0) return 100;
    
    return 100 - ((total_allocated * 100) / total_blocks);
}

void* MemoryPoolManager::emergency_allocate(size_t size) {
    if (size > EMERGENCY_RESERVE_SIZE || emergency_used_.exchange(true)) {
        return nullptr;
    }
    
    ESP_LOGW(TAG, "Using emergency reserve for %zu bytes", size);
    return emergency_reserve_;
}

// JSON export implementation
nlohmann::json PoolDiagnostics::to_json() const {
    nlohmann::json j;
    j["current_usage"] = current_usage;
    j["peak_usage"] = peak_usage;
    j["total_allocations"] = total_allocations;
    j["allocation_failures"] = allocation_failures;
    j["fragmentation_index"] = fragmentation_index;
    j["low_memory_warning"] = low_memory_warning;
    j["critical_memory_alert"] = critical_memory_alert;
    
    nlohmann::json pools_array = nlohmann::json::array();
    for (int i = 0; i < 5; i++) {
        nlohmann::json pool;
        pool["name"] = pools[i].name;
        pool["block_size"] = pools[i].block_size;
        pool["total_blocks"] = pools[i].total_blocks;
        pool["used_blocks"] = pools[i].used_blocks;
        pool["allocation_rate"] = pools[i].allocation_rate;
        pool["avg_hold_time_ms"] = pools[i].avg_hold_time_ms;
        pool["peak_used_blocks"] = pools[i].peak_used_blocks;
        pools_array.push_back(pool);
    }
    j["pools"] = pools_array;
    
    return j;
}

// Global convenience functions
esp_err_t init(AllocationStrategy strategy) {
    return MemoryPoolManager::get_instance().init(strategy);
}

void* allocate(size_t size) {
    return MemoryPoolManager::get_instance().allocate(size);
}

esp_err_t deallocate(void* ptr, size_t size) {
    return MemoryPoolManager::get_instance().deallocate(ptr, size);
}

PoolDiagnostics get_diagnostics() {
    return MemoryPoolManager::get_instance().get_diagnostics();
}

} // namespace MemoryPool
