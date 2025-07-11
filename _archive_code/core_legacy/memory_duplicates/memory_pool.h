/**
 * @file memory_pool.h
 * @brief Deterministic memory allocation system for industrial embedded applications
 * 
 * Provides fixed-size memory pools with O(1) allocation/deallocation,
 * zero heap fragmentation, and comprehensive diagnostics.
 * 
 * Designed for ESP32 industrial refrigeration control systems.
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <array>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nlohmann/json.hpp"

namespace MemoryPool {

/**
 * @brief Standard block sizes for industrial applications
 */
enum class BlockSize : uint16_t {
    TINY   = 32,    // Event headers, small commands
    SMALL  = 64,    // Standard Events with data
    MEDIUM = 128,   // JSON payloads, control messages
    LARGE  = 256,   // Sensor data batches
    XLARGE = 512    // Web responses, config blocks
};

/**
 * @brief Memory allocation strategies
 */
enum class AllocationStrategy {
    STRICT_NO_FALLBACK,    // Industrial mode - fail fast
    FALLBACK_TO_HEAP,      // Debug/development only
    WAIT_WITH_TIMEOUT      // Critical operations with timeout
};

/**
 * @brief Pool configuration parameters
 */
struct PoolConfig {
    uint16_t tiny_blocks   = 128;  // Many small messages
    uint16_t small_blocks  = 64;   // Standard events
    uint16_t medium_blocks = 32;   // Commands and responses
    uint16_t large_blocks  = 16;   // Sensor data
    uint16_t xlarge_blocks = 8;    // Web/config data
    
    size_t get_total_memory() const {
        return (tiny_blocks * static_cast<size_t>(BlockSize::TINY)) +
               (small_blocks * static_cast<size_t>(BlockSize::SMALL)) +
               (medium_blocks * static_cast<size_t>(BlockSize::MEDIUM)) +
               (large_blocks * static_cast<size_t>(BlockSize::LARGE)) +
               (xlarge_blocks * static_cast<size_t>(BlockSize::XLARGE));
    }
};

/**
 * @brief Memory block header for tracking
 */
struct BlockHeader {
    uint16_t pool_id : 3;      // Which pool (0-7)
    uint16_t allocated : 1;    // Allocation status
    uint16_t magic : 12;       // Magic number for corruption detection
    uint16_t next_free;        // Next free block index
    
    static constexpr uint16_t MAGIC_FREE = 0xDEA;
    static constexpr uint16_t MAGIC_USED = 0xBEE;
};

/**
 * @brief Pool diagnostics structure
 */
struct PoolDiagnostics {
    // Runtime statistics
    uint32_t current_usage;
    uint32_t peak_usage;
    uint32_t total_allocations;
    uint32_t allocation_failures;
    uint32_t fragmentation_index;  // 0-100%
    
    // Per-pool metrics
    struct PoolMetrics {
        const char* name;
        size_t block_size;
        uint16_t total_blocks;
        uint16_t used_blocks;
        uint32_t allocation_rate;  // per second
        uint32_t avg_hold_time_ms;
        uint32_t peak_used_blocks;
    } pools[5];
    
    // Alerts
    bool low_memory_warning;    // < 20% free
    bool critical_memory_alert; // < 5% free
    
    // JSON export for Web UI
    nlohmann::json to_json() const;
};

/**
 * @brief Static memory pool with lock-free allocation
 * 
 * @tparam BlockSizeValue Size of each block
 * @tparam BlockCount Number of blocks in pool
 */
template<size_t BlockSizeValue, size_t BlockCount>
class StaticMemoryPool {
private:
    static constexpr uint16_t INVALID_INDEX = 0xFFFF;
    static constexpr size_t ACTUAL_BLOCK_SIZE = BlockSizeValue + sizeof(BlockHeader);
    
    // Static memory - no heap allocation
    alignas(8) uint8_t memory_[ACTUAL_BLOCK_SIZE * BlockCount];
    
    // Free list management
    std::atomic<uint16_t> free_head_{0};
    
    // Statistics (lock-free)
    std::atomic<uint32_t> allocated_count_{0};
    std::atomic<uint32_t> peak_usage_{0};
    std::atomic<uint32_t> allocation_failures_{0};
    std::atomic<uint64_t> total_allocations_{0};
    
    // Thread safety
    SemaphoreHandle_t mutex_;
    
public:
    StaticMemoryPool() {
        mutex_ = xSemaphoreCreateMutex();
        initialize_free_list();
    }
    
    ~StaticMemoryPool() {
        if (mutex_) {
            vSemaphoreDelete(mutex_);
        }
    }
    
    /**
     * @brief Allocate a block from pool
     * @return Pointer to allocated memory or nullptr if pool exhausted
     */
    void* allocate() {
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
            allocation_failures_++;
            return nullptr;
        }
        
        uint16_t head = free_head_.load();
        if (head == INVALID_INDEX) {
            xSemaphoreGive(mutex_);
            allocation_failures_++;
            return nullptr;
        }
        
        BlockHeader* header = get_header(head);
        free_head_.store(header->next_free);
        
        header->allocated = 1;
        header->magic = BlockHeader::MAGIC_USED;
        
        allocated_count_++;
        total_allocations_++;
        update_peak_usage();
        
        xSemaphoreGive(mutex_);
        
        return get_user_ptr(head);
    }
    
    /**
     * @brief Deallocate a block back to pool
     * @param ptr Pointer to deallocate
     * @return ESP_OK on success
     */
    esp_err_t deallocate(void* ptr) {
        if (!ptr) return ESP_ERR_INVALID_ARG;
        
        if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(10)) != pdTRUE) {
            return ESP_ERR_TIMEOUT;
        }
        
        uint16_t index = get_block_index(ptr);
        if (index >= BlockCount) {
            xSemaphoreGive(mutex_);
            return ESP_ERR_INVALID_ARG;
        }
        
        BlockHeader* header = get_header(index);
        if (header->magic != BlockHeader::MAGIC_USED || !header->allocated) {
            xSemaphoreGive(mutex_);
            return ESP_ERR_INVALID_STATE;
        }
        
        // Clear user data for security
        memset(ptr, 0, BlockSizeValue);
        
        header->allocated = 0;
        header->magic = BlockHeader::MAGIC_FREE;
        header->next_free = free_head_.load();
        free_head_.store(index);
        
        allocated_count_--;
        xSemaphoreGive(mutex_);
        
        return ESP_OK;
    }
    
    // Statistics methods
    uint32_t get_allocated_count() const { return allocated_count_.load(); }
    uint32_t get_peak_usage() const { return peak_usage_.load(); }
    uint32_t get_allocation_failures() const { return allocation_failures_.load(); }
    uint32_t get_total_blocks() const { return BlockCount; }
    size_t get_block_size() const { return BlockSizeValue; }
    
private:
    void initialize_free_list() {
        for (uint16_t i = 0; i < BlockCount; i++) {
            BlockHeader* header = get_header(i);
            header->pool_id = 0;
            header->allocated = 0;
            header->magic = BlockHeader::MAGIC_FREE;
            header->next_free = (i < BlockCount - 1) ? i + 1 : INVALID_INDEX;
        }
    }
    
    BlockHeader* get_header(uint16_t index) {
        return reinterpret_cast<BlockHeader*>(&memory_[index * ACTUAL_BLOCK_SIZE]);
    }
    
    void* get_user_ptr(uint16_t index) {
        return &memory_[index * ACTUAL_BLOCK_SIZE + sizeof(BlockHeader)];
    }
    
    uint16_t get_block_index(void* ptr) {
        uint8_t* user_ptr = static_cast<uint8_t*>(ptr);
        size_t offset = user_ptr - memory_ - sizeof(BlockHeader);
        return offset / ACTUAL_BLOCK_SIZE;
    }
    
    void update_peak_usage() {
        uint32_t current = allocated_count_.load();
        uint32_t peak = peak_usage_.load();
        while (current > peak && !peak_usage_.compare_exchange_weak(peak, current));
    }
};

/**
 * @brief Central memory pool manager
 */
class MemoryPoolManager {
private:
    static MemoryPoolManager* instance_;
    
    // Individual pools
    StaticMemoryPool<32, CONFIG_MEMORY_POOL_TINY_COUNT> tiny_pool_;
    StaticMemoryPool<64, CONFIG_MEMORY_POOL_SMALL_COUNT> small_pool_;
    StaticMemoryPool<128, CONFIG_MEMORY_POOL_MEDIUM_COUNT> medium_pool_;
    StaticMemoryPool<256, CONFIG_MEMORY_POOL_LARGE_COUNT> large_pool_;
    StaticMemoryPool<512, CONFIG_MEMORY_POOL_XLARGE_COUNT> xlarge_pool_;
    
    // Configuration
    AllocationStrategy default_strategy_;
    
    // Emergency reserve
    static constexpr size_t EMERGENCY_RESERVE_SIZE = 1024;
    alignas(8) uint8_t emergency_reserve_[EMERGENCY_RESERVE_SIZE];
    std::atomic<bool> emergency_used_{false};
    
    // Alert thresholds
    uint8_t low_memory_threshold_ = 80;  // percentage
    uint8_t critical_memory_threshold_ = 95;  // percentage
    
    MemoryPoolManager() : default_strategy_(AllocationStrategy::STRICT_NO_FALLBACK) {}
    
public:
    static MemoryPoolManager& get_instance() {
        if (!instance_) {
            instance_ = new MemoryPoolManager();
        }
        return *instance_;
    }
    
    /**
     * @brief Initialize memory pool system
     */
    esp_err_t init(AllocationStrategy strategy = AllocationStrategy::STRICT_NO_FALLBACK);
    
    /**
     * @brief Allocate memory from appropriate pool
     */
    void* allocate(size_t size, AllocationStrategy strategy = AllocationStrategy::STRICT_NO_FALLBACK);
    
    /**
     * @brief Deallocate memory back to pool
     */
    esp_err_t deallocate(void* ptr, size_t size);
    
    /**
     * @brief Get pool diagnostics
     */
    PoolDiagnostics get_diagnostics() const;
    
    /**
     * @brief Check if system has low memory
     */
    bool is_low_memory() const;
    
    /**
     * @brief Get percentage of free memory
     */
    uint8_t get_free_percentage() const;
    
    /**
     * @brief Emergency allocation (last resort)
     */
    void* emergency_allocate(size_t size);
    
    /**
     * @brief Select appropriate pool for size
     */
    BlockSize select_pool_size(size_t requested_size) const;
};

// Convenience functions for global access
esp_err_t init(AllocationStrategy strategy = AllocationStrategy::STRICT_NO_FALLBACK);
void* allocate(size_t size);
esp_err_t deallocate(void* ptr, size_t size);
PoolDiagnostics get_diagnostics();

} // namespace MemoryPool

#endif // MEMORY_POOL_H
