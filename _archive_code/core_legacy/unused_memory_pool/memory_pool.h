/**
 * @file memory_pool.h
 * @brief Industrial-grade memory pool system for ModESP
 * 
 * Provides deterministic memory allocation with zero fragmentation,
 * suitable for 24/7 industrial refrigeration control systems.
 * 
 * Features:
 * - O(1) lock-free allocation/deallocation
 * - Fixed-size memory blocks
 * - No heap fragmentation
 * - Comprehensive diagnostics
 * - Thread-safe operations
 * 
 * @author ModESP Team
 * @date 2024
 */

#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <memory>
#include <array>
#include <functional>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace ModESP {
namespace Memory {

/**
 * @brief Standard pool sizes for industrial embedded systems
 */
enum class PoolSize : uint16_t {
    TINY   = 32,    ///< Event headers, small commands
    SMALL  = 64,    ///< Standard Events with data
    MEDIUM = 128,   ///< JSON payloads, control messages
    LARGE  = 256,   ///< Sensor data batches
    XLARGE = 512    ///< Web responses, config blocks
};

/**
 * @brief Memory allocation strategies
 */
enum class AllocationStrategy {
    STRICT_NO_FALLBACK,    ///< Industrial mode - fail fast (default)
    FALLBACK_TO_HEAP,      ///< Debug/development only
    WAIT_WITH_TIMEOUT      ///< Critical operations with timeout
};

/**
 * @brief Pool configuration parameters
 */
struct PoolConfig {
    uint16_t tiny_blocks   = 128;  ///< Number of 32-byte blocks
    uint16_t small_blocks  = 64;   ///< Number of 64-byte blocks
    uint16_t medium_blocks = 32;   ///< Number of 128-byte blocks
    uint16_t large_blocks  = 16;   ///< Number of 256-byte blocks
    uint16_t xlarge_blocks = 8;    ///< Number of 512-byte blocks
    
    /**
     * @brief Calculate total memory requirement
     */
    size_t get_total_memory() const {
        return (tiny_blocks * static_cast<size_t>(PoolSize::TINY)) +
               (small_blocks * static_cast<size_t>(PoolSize::SMALL)) +
               (medium_blocks * static_cast<size_t>(PoolSize::MEDIUM)) +
               (large_blocks * static_cast<size_t>(PoolSize::LARGE)) +
               (xlarge_blocks * static_cast<size_t>(PoolSize::XLARGE));
    }
};

/**
 * @brief Pool statistics for monitoring
 */
struct PoolStats {
    uint32_t allocated_count;      ///< Currently allocated blocks
    uint32_t peak_usage;          ///< Maximum blocks ever allocated
    uint32_t total_allocations;   ///< Total allocation operations
    uint32_t allocation_failures; ///< Failed allocation attempts
    uint32_t average_hold_time_ms;///< Average block usage duration
    uint64_t total_bytes_served;  ///< Total bytes allocated over time
};

/**
 * @brief Invalid index marker
 */
static constexpr uint16_t INVALID_INDEX = 0xFFFF;

/**
 * @brief Lock-free memory pool implementation
 * 
 * Thread-safe, deterministic O(1) allocation/deallocation
 * Uses atomic operations for lock-free behavior
 * 
 * @tparam BlockSize Size of each memory block
 * @tparam BlockCount Number of blocks in pool
 */
template<size_t BlockSize, size_t BlockCount>
class StaticMemoryPool {
    static_assert(BlockSize >= 8, "Block size must be at least 8 bytes");
    static_assert(BlockCount > 0, "Block count must be greater than 0");
    static_assert(BlockCount < INVALID_INDEX, "Block count exceeds maximum");

private:
    /// Aligned memory storage
    alignas(8) uint8_t memory_[BlockSize * BlockCount];
    
    /// Free list implemented as lock-free stack
    std::atomic<uint16_t> free_list_[BlockCount];
    std::atomic<uint16_t> free_head_;
    
    /// Statistics (atomic for thread safety)
    std::atomic<uint32_t> allocated_count_;
    std::atomic<uint32_t> peak_usage_;
    std::atomic<uint32_t> total_allocations_;
    std::atomic<uint32_t> allocation_failures_;
    
    /// Allocation timestamps for hold time calculation
    uint32_t allocation_time_[BlockCount];
    
    /// Debug mode - poison freed memory
    #ifdef CONFIG_MEMORY_POOL_POISON_FREED
    static constexpr uint8_t POISON_VALUE = 0xDE;
    #endif
    
    /**
     * @brief Update peak usage statistic
     */
    void update_peak_usage() {
        uint32_t current = allocated_count_.load();
        uint32_t peak = peak_usage_.load();
        while (current > peak && !peak_usage_.compare_exchange_weak(peak, current));
    }

public:
    /**
     * @brief Initialize memory pool
     */
    StaticMemoryPool() : free_head_(0), allocated_count_(0), 
                        peak_usage_(0), total_allocations_(0), 
                        allocation_failures_(0) {
        // Initialize free list as linked stack
        for (uint16_t i = 0; i < BlockCount - 1; ++i) {
            free_list_[i] = i + 1;
        }
        free_list_[BlockCount - 1] = INVALID_INDEX;
        
        // Clear allocation timestamps
        memset(allocation_time_, 0, sizeof(allocation_time_));
    }

    /**
     * @brief Allocate a memory block
     * 
     * @return Pointer to allocated block or nullptr if pool exhausted
     */
    void* allocate() {
        uint16_t head = free_head_.load(std::memory_order_acquire);
        
        while (head != INVALID_INDEX) {
            uint16_t next = free_list_[head].load(std::memory_order_relaxed);
            
            if (free_head_.compare_exchange_weak(head, next, 
                                                std::memory_order_release,
                                                std::memory_order_acquire)) {
                // Success - update statistics
                allocated_count_.fetch_add(1, std::memory_order_relaxed);
                total_allocations_.fetch_add(1, std::memory_order_relaxed);
                update_peak_usage();
                
                // Record allocation time
                allocation_time_[head] = xTaskGetTickCount();
                
                return &memory_[head * BlockSize];
            }
            // CAS failed, retry with new head value
        }
        
        // Pool exhausted
        allocation_failures_.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }

    /**
     * @brief Deallocate a memory block
     * 
     * @param ptr Pointer to block to deallocate
     * @return true if successful, false if invalid pointer
     */
    bool deallocate(void* ptr) {
        if (!ptr) return false;
        
        // Validate pointer is within our pool
        uint8_t* byte_ptr = static_cast<uint8_t*>(ptr);
        if (byte_ptr < memory_ || byte_ptr >= memory_ + sizeof(memory_)) {
            ESP_LOGE("MemoryPool", "Invalid pointer in deallocate");
            return false;
        }
        
        // Calculate block index
        size_t offset = byte_ptr - memory_;
        if (offset % BlockSize != 0) {
            ESP_LOGE("MemoryPool", "Misaligned pointer in deallocate");
            return false;
        }
        
        uint16_t index = offset / BlockSize;
        
        #ifdef CONFIG_MEMORY_POOL_POISON_FREED
        // Poison memory for debug
        memset(ptr, POISON_VALUE, BlockSize);
        #endif
        
        // Return block to free list
        uint16_t head = free_head_.load(std::memory_order_acquire);
        do {
            free_list_[index].store(head, std::memory_order_relaxed);
        } while (!free_head_.compare_exchange_weak(head, index,
                                                   std::memory_order_release,
                                                   std::memory_order_acquire));
        
        allocated_count_.fetch_sub(1, std::memory_order_relaxed);
        return true;
    }

    /**
     * @brief Get current pool statistics
     */
    PoolStats get_stats() const {
        PoolStats stats;
        stats.allocated_count = allocated_count_.load();
        stats.peak_usage = peak_usage_.load();
        stats.total_allocations = total_allocations_.load();
        stats.allocation_failures = allocation_failures_.load();
        stats.total_bytes_served = stats.total_allocations * BlockSize;
        
        // Calculate average hold time
        // TODO: Implement hold time tracking
        stats.average_hold_time_ms = 0;
        
        return stats;
    }

    /**
     * @brief Get number of free blocks
     */
    uint16_t get_free_blocks() const {
        return BlockCount - allocated_count_.load();
    }

    /**
     * @brief Get pool utilization percentage
     */
    uint8_t get_utilization_percent() const {
        return (allocated_count_.load() * 100) / BlockCount;
    }

    /**
     * @brief Check if pool is exhausted
     */
    bool is_exhausted() const {
        return free_head_.load() == INVALID_INDEX;
    }

    /**
     * @brief Reset statistics (not the pool itself)
     */
    void reset_stats() {
        peak_usage_ = allocated_count_.load();
        total_allocations_ = 0;
        allocation_failures_ = 0;
    }
};

/**
 * @brief Pooled allocation wrapper with automatic deallocation
 * 
 * RAII wrapper that automatically returns memory to pool on destruction
 */
template<typename T>
class PooledPtr {
private:
    T* ptr_;
    std::function<void(void*)> deleter_;

public:
    PooledPtr() : ptr_(nullptr) {}
    
    PooledPtr(T* ptr, std::function<void(void*)> deleter) 
        : ptr_(ptr), deleter_(deleter) {}
    
    ~PooledPtr() {
        if (ptr_ && deleter_) {
            ptr_->~T(); // Call destructor
            deleter_(ptr_);
        }
    }
    
    // Move semantics
    PooledPtr(PooledPtr&& other) noexcept 
        : ptr_(other.ptr_), deleter_(std::move(other.deleter_)) {
        other.ptr_ = nullptr;
    }
    
    PooledPtr& operator=(PooledPtr&& other) noexcept {
        if (this != &other) {
            if (ptr_ && deleter_) {
                ptr_->~T();
                deleter_(ptr_);
            }
            ptr_ = other.ptr_;
            deleter_ = std::move(other.deleter_);
            other.ptr_ = nullptr;
        }
        return *this;
    }
    
    // Delete copy operations
    PooledPtr(const PooledPtr&) = delete;
    PooledPtr& operator=(const PooledPtr&) = delete;
    
    // Pointer operations
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    T& operator*() { return *ptr_; }
    const T& operator*() const { return *ptr_; }
    T* get() { return ptr_; }
    const T* get() const { return ptr_; }
    
    explicit operator bool() const { return ptr_ != nullptr; }
    
    /**
     * @brief Release ownership without deallocating
     */
    T* release() {
        T* temp = ptr_;
        ptr_ = nullptr;
        return temp;
    }
};

// Forward declaration
class MemoryPoolManager;

/**
 * @brief Central memory pool manager
 * 
 * Manages multiple pools of different sizes and provides
 * unified interface for memory allocation
 */
class MemoryPoolManager {
private:
    // Memory pools for different sizes - use appropriate config values
    StaticMemoryPool<32, 128> tiny_pool_;    // CONFIG_MEMORY_POOL_TINY_COUNT
    StaticMemoryPool<64, 64> small_pool_;    // CONFIG_MEMORY_POOL_SMALL_COUNT
    StaticMemoryPool<128, 32> medium_pool_;  // CONFIG_MEMORY_POOL_MEDIUM_COUNT
    StaticMemoryPool<256, 16> large_pool_;   // CONFIG_MEMORY_POOL_LARGE_COUNT
    StaticMemoryPool<512, 8> xlarge_pool_;   // CONFIG_MEMORY_POOL_XLARGE_COUNT
    
    // Emergency reserve for critical operations
    #ifdef CONFIG_MEMORY_POOL_EMERGENCY_RESERVE
    static constexpr size_t EMERGENCY_RESERVE_SIZE = 2048;
    alignas(8) uint8_t emergency_reserve_[EMERGENCY_RESERVE_SIZE];
    std::atomic<bool> emergency_used_;
    SemaphoreHandle_t emergency_mutex_;
    #endif
    
    // Global statistics
    std::atomic<uint32_t> total_allocation_requests_;
    std::atomic<uint32_t> total_allocation_failures_;
    std::atomic<bool> low_memory_alert_;
    std::atomic<bool> critical_memory_alert_;
    
    // Allocation strategy
    AllocationStrategy default_strategy_;
    
    /**
     * @brief Select appropriate pool for size
     */
    void* allocate_from_pool(size_t size) {
        if (size <= 32) return tiny_pool_.allocate();
        if (size <= 64) return small_pool_.allocate();
        if (size <= 128) return medium_pool_.allocate();
        if (size <= 256) return large_pool_.allocate();
        if (size <= 512) return xlarge_pool_.allocate();
        return nullptr; // Too large
    }
    
    /**
     * @brief Deallocate to appropriate pool
     */
    bool deallocate_to_pool(void* ptr, size_t size) {
        if (size <= 32) return tiny_pool_.deallocate(ptr);
        if (size <= 64) return small_pool_.deallocate(ptr);
        if (size <= 128) return medium_pool_.deallocate(ptr);
        if (size <= 256) return large_pool_.deallocate(ptr);
        if (size <= 512) return xlarge_pool_.deallocate(ptr);
        return false;
    }
    
    /**
     * @brief Check and update memory alerts
     */
    void update_memory_alerts() {
        uint8_t overall_usage = get_overall_utilization();
        
        // Use CONFIG_MEMORY_POOL_ALERT_THRESHOLD if defined, otherwise default
        #ifdef CONFIG_MEMORY_POOL_ALERT_THRESHOLD
        low_memory_alert_ = (overall_usage >= CONFIG_MEMORY_POOL_ALERT_THRESHOLD);
        #else
        low_memory_alert_ = (overall_usage >= 80);
        #endif
        
        #ifdef CONFIG_MEMORY_POOL_CRITICAL_THRESHOLD
        critical_memory_alert_ = (overall_usage >= CONFIG_MEMORY_POOL_CRITICAL_THRESHOLD);
        #else
        critical_memory_alert_ = (overall_usage >= 95);
        #endif
        
        if (critical_memory_alert_) {
            ESP_LOGE("MemoryPool", "CRITICAL: Memory usage at %d%%", overall_usage);
        } else if (low_memory_alert_) {
            ESP_LOGW("MemoryPool", "WARNING: Memory usage at %d%%", overall_usage);
        }
    }

public:
    /**
     * @brief Initialize pool manager
     */
    MemoryPoolManager() : 
        #ifdef CONFIG_MEMORY_POOL_EMERGENCY_RESERVE
        emergency_used_(false),
        #endif
        total_allocation_requests_(0),
        total_allocation_failures_(0),
        low_memory_alert_(false),
        critical_memory_alert_(false),
        default_strategy_(AllocationStrategy::STRICT_NO_FALLBACK) {
        
        #ifdef CONFIG_MEMORY_POOL_EMERGENCY_RESERVE
        emergency_mutex_ = xSemaphoreCreateMutex();
        #endif
        
        ESP_LOGI("MemoryPool", "Initialized with %zu bytes total capacity",
                 get_total_capacity());
    }
    
    ~MemoryPoolManager() {
        #ifdef CONFIG_MEMORY_POOL_EMERGENCY_RESERVE
        if (emergency_mutex_) {
            vSemaphoreDelete(emergency_mutex_);
        }
        #endif
    }
    
    /**
     * @brief Allocate memory block
     * 
     * @param size Required size in bytes
     * @param strategy Allocation strategy
     * @param timeout_ms Timeout for WAIT_WITH_TIMEOUT strategy
     * @return Pointer to allocated memory or nullptr
     */
    void* allocate(size_t size, 
                   AllocationStrategy strategy = AllocationStrategy::STRICT_NO_FALLBACK,
                   uint32_t timeout_ms = 100) {
        total_allocation_requests_.fetch_add(1);
        
        // Try pool allocation first
        void* ptr = allocate_from_pool(size);
        if (ptr) {
            update_memory_alerts();
            return ptr;
        }
        
        // Handle allocation failure based on strategy
        total_allocation_failures_.fetch_add(1);
        
        switch (strategy) {
            case AllocationStrategy::STRICT_NO_FALLBACK:
                ESP_LOGE("MemoryPool", "Pool exhausted for size %zu", size);
                return nullptr;
                
            case AllocationStrategy::WAIT_WITH_TIMEOUT: {
                uint32_t start = xTaskGetTickCount();
                while ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(timeout_ms)) {
                    ptr = allocate_from_pool(size);
                    if (ptr) {
                        update_memory_alerts();
                        return ptr;
                    }
                    vTaskDelay(pdMS_TO_TICKS(1));
                }
                ESP_LOGE("MemoryPool", "Timeout waiting for memory (size %zu)", size);
                return nullptr;
            }
            
            case AllocationStrategy::FALLBACK_TO_HEAP:
                #ifdef CONFIG_MEMORY_POOL_ALLOW_HEAP_FALLBACK
                ESP_LOGW("MemoryPool", "Falling back to heap for size %zu", size);
                return malloc(size);
                #else
                ESP_LOGE("MemoryPool", "Heap fallback disabled in production");
                return nullptr;
                #endif
        }
        
        return nullptr;
    }
    
    /**
     * @brief Allocate and construct object
     */
    template<typename T, typename... Args>
    PooledPtr<T> allocate_object(Args&&... args) {
        void* memory = allocate(sizeof(T), default_strategy_);
        if (!memory) {
            return PooledPtr<T>();
        }
        
        T* obj = new(memory) T(std::forward<Args>(args)...);
        
        auto deleter = [this](void* ptr) {
            this->deallocate(ptr, sizeof(T));
        };
        
        return PooledPtr<T>(obj, deleter);
    }
    
    /**
     * @brief Deallocate memory block
     */
    bool deallocate(void* ptr, size_t size) {
        if (!ptr) return false;
        
        bool result = deallocate_to_pool(ptr, size);
        if (result) {
            update_memory_alerts();
        }
        return result;
    }
    
    /**
     * @brief Get overall memory utilization percentage
     */
    uint8_t get_overall_utilization() const {
        size_t total_used = tiny_pool_.get_stats().allocated_count * 32 +
                           small_pool_.get_stats().allocated_count * 64 +
                           medium_pool_.get_stats().allocated_count * 128 +
                           large_pool_.get_stats().allocated_count * 256 +
                           xlarge_pool_.get_stats().allocated_count * 512;
        
        return (total_used * 100) / get_total_capacity();
    }
    
    /**
     * @brief Get total memory capacity
     */
    size_t get_total_capacity() const {
        return 128 * 32 +   // tiny
               64 * 64 +    // small
               32 * 128 +   // medium
               16 * 256 +   // large
               8 * 512;     // xlarge
    }
    
    /**
     * @brief Emergency allocation for critical operations
     */
    #ifdef CONFIG_MEMORY_POOL_EMERGENCY_RESERVE
    void* emergency_allocate(size_t size) {
        if (size > EMERGENCY_RESERVE_SIZE) return nullptr;
        
        if (xSemaphoreTake(emergency_mutex_, portMAX_DELAY) == pdTRUE) {
            if (!emergency_used_ && size <= EMERGENCY_RESERVE_SIZE) {
                emergency_used_ = true;
                xSemaphoreGive(emergency_mutex_);
                ESP_LOGW("MemoryPool", "Using emergency reserve (%zu bytes)", size);
                return emergency_reserve_;
            }
            xSemaphoreGive(emergency_mutex_);
        }
        return nullptr;
    }
    #endif
    
    /**
     * @brief Set default allocation strategy
     */
    void set_default_strategy(AllocationStrategy strategy) {
        default_strategy_ = strategy;
    }
    
    /**
     * @brief Get memory alerts status
     */
    bool has_low_memory_alert() const { return low_memory_alert_; }
    bool has_critical_memory_alert() const { return critical_memory_alert_; }
    
    /**
     * @brief Get singleton instance
     */
    static MemoryPoolManager& instance() {
        static MemoryPoolManager manager;
        return manager;
    }
    
    // Make pools accessible for diagnostics
    StaticMemoryPool<32, 128>& get_tiny_pool() { return tiny_pool_; }
    StaticMemoryPool<64, 64>& get_small_pool() { return small_pool_; }
    StaticMemoryPool<128, 32>& get_medium_pool() { return medium_pool_; }
    StaticMemoryPool<256, 16>& get_large_pool() { return large_pool_; }
    StaticMemoryPool<512, 8>& get_xlarge_pool() { return xlarge_pool_; }
};

// Convenience function for singleton access
inline MemoryPoolManager& get_pool_manager() {
    return MemoryPoolManager::instance();
}

// Global convenience allocator functions
template<typename T, typename... Args>
inline PooledPtr<T> make_pooled(Args&&... args) {
    return get_pool_manager().allocate_object<T>(std::forward<Args>(args)...);
}

} // namespace Memory
} // namespace ModESP

#endif // MEMORY_POOL_H
