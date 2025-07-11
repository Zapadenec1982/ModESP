# Memory Pool Integration Guide

## Quick Start for Module Developers

This guide shows how to integrate the Memory Pool system into your ModESP modules.

### 1. Basic Usage in Modules

```cpp
#include "memory_pool.h"
#include "base_module.h"

class MyModule : public BaseModule {
private:
    // Use pooled allocation for module data
    ModESP::Memory::PooledPtr<SensorBuffer> buffer_;
    
public:
    void init() override {
        BaseModule::init();
        
        // Allocate buffer from pool
        buffer_ = ModESP::make_pooled<SensorBuffer>();
        if (!buffer_) {
            ESP_LOGE(TAG, "Failed to allocate sensor buffer");
            // Handle gracefully
        }
    }
    
    void process_data() {
        // Temporary allocation
        auto temp_data = ModESP::make_pooled<ProcessingData>();
        if (temp_data) {
            // Use temp_data...
            // Automatically deallocated on scope exit
        }
    }
};
```

### 2. Event Publishing with Pooled Memory

```cpp
void MyModule::publish_sensor_event(float value) {
    // Use pooled event publishing
    esp_err_t err = ModESP::PooledEventBus::publish_pooled(
        "sensor.temperature.updated",
        {
            {"module_id", get_id()},
            {"value", value},
            {"timestamp", esp_timer_get_time()},
            {"unit", "celsius"}
        },
        EventBus::Priority::NORMAL
    );
    
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to publish event: %s", esp_err_to_name(err));
    }
}
```

### 3. Handling Memory Pressure

```cpp
class MemoryAwareModule : public BaseModule {
private:
    bool low_memory_mode_ = false;
    
public:
    void init() override {
        BaseModule::init();
        
        // Register for memory alerts
        auto& pool_manager = ModESP::Memory::get_pool_manager();
        
        // Check memory periodically
        create_recurring_task("memory_check", 5000, [this]() {
            this->check_memory_status();
        });
    }
    
    void check_memory_status() {
        auto& pm = ModESP::Memory::get_pool_manager();
        uint8_t usage = pm.get_overall_utilization();
        
        if (usage > 80 && !low_memory_mode_) {
            ESP_LOGW(TAG, "Entering low memory mode (usage: %d%%)", usage);
            low_memory_mode_ = true;
            reduce_memory_usage();
        } else if (usage < 60 && low_memory_mode_) {
            ESP_LOGI(TAG, "Exiting low memory mode");
            low_memory_mode_ = false;
        }
    }
    
    void reduce_memory_usage() {
        // Implement memory-saving strategies
        // - Reduce buffer sizes
        // - Increase data aggregation intervals
        // - Disable non-critical features
    }
};
```

### 4. Module-Specific Memory Pools

For modules that need dedicated memory:

```cpp
class DataLoggerModule : public BaseModule {
private:
    // Module-specific pool for log entries
    ModESP::Memory::StaticMemoryPool<sizeof(LogEntry), 100> log_pool_;
    
public:
    void log_data(const SensorData& data) {
        void* mem = log_pool_.allocate();
        if (mem) {
            LogEntry* entry = new(mem) LogEntry{
                .timestamp = esp_timer_get_time(),
                .sensor_id = data.sensor_id,
                .value = data.value
            };
            
            // Process entry...
            
            // Return to pool
            entry->~LogEntry();
            log_pool_.deallocate(mem);
        }
    }
};
```

### 5. Best Practices for Modules

#### DO:
- ✅ Use `ModESP::make_pooled<T>()` for automatic memory management
- ✅ Check allocation results and handle failures gracefully
- ✅ Keep allocations short-lived
- ✅ Monitor module memory usage
- ✅ Implement degraded operation modes for low memory

#### DON'T:
- ❌ Hold pool memory across long operations
- ❌ Allocate in ISRs (use pre-allocated buffers)
- ❌ Ignore allocation failures
- ❌ Mix pool and heap allocations for same data type

### 6. Common Patterns

#### Ring Buffer with Pooled Blocks
```cpp
class PooledRingBuffer {
    struct Block {
        uint8_t data[256];
        uint32_t timestamp;
    };
    
    std::vector<ModESP::Memory::PooledPtr<Block>> blocks_;
    size_t write_idx_ = 0;
    
public:
    void add_data(const uint8_t* data, size_t len) {
        auto block = ModESP::make_pooled<Block>();
        if (block) {
            memcpy(block->data, data, std::min(len, sizeof(Block::data)));
            block->timestamp = esp_timer_get_time();
            
            if (write_idx_ >= blocks_.size()) {
                blocks_.push_back(std::move(block));
            } else {
                blocks_[write_idx_] = std::move(block);
            }
            
            write_idx_ = (write_idx_ + 1) % MAX_BLOCKS;
        }
    }
};
```

#### Command Queue with Pooled Commands
```cpp
class CommandQueue {
    struct Command {
        enum Type { READ, WRITE, CONTROL };
        Type type;
        uint32_t address;
        uint8_t data[64];
    };
    
    QueueHandle_t queue_;
    
public:
    void enqueue_command(Command::Type type, uint32_t addr) {
        auto cmd = ModESP::make_pooled<Command>();
        if (cmd) {
            cmd->type = type;
            cmd->address = addr;
            
            // Queue takes ownership
            Command* raw = cmd.release();
            if (xQueueSend(queue_, &raw, 0) != pdTRUE) {
                // Failed to queue, clean up
                ModESP::Memory::get_pool_manager().deallocate(raw, sizeof(Command));
            }
        }
    }
};
```

### 7. Debugging Memory Issues

```cpp
void debug_module_memory() {
    ModESP::Memory::MemoryDiagnostics diag(
        ModESP::Memory::get_pool_manager()
    );
    
    // Log detailed statistics
    diag.log_stats(TAG);
    
    // Check for potential leaks
    auto leaks = diag.detect_potential_leaks(30000); // 30s threshold
    if (!leaks.empty()) {
        ESP_LOGW(TAG, "Potential memory leaks detected: %zu", leaks.size());
    }
    
    // Get pool-specific stats
    auto stats = ModESP::PooledEventBus::get_event_pool_stats();
    ESP_LOGI(TAG, "Event pool: %u allocated, %u failures", 
             stats.allocated_count, stats.allocation_failures);
}
```

### 8. Migration from Heap Allocation

Before (heap):
```cpp
void process() {
    auto* buffer = new uint8_t[256];
    // Use buffer...
    delete[] buffer;
}
```

After (pool):
```cpp
void process() {
    auto buffer = ModESP::make_pooled<std::array<uint8_t, 256>>();
    if (buffer) {
        // Use buffer...
        // Automatic cleanup
    }
}
```

### 9. Performance Monitoring

```cpp
void benchmark_module_allocations() {
    TRACK_MEMORY_USAGE("Module Processing");
    
    // Your module operations...
    
    // Automatic performance logging on scope exit
}
```

### 10. Configuration Tips

For module-heavy systems, adjust pool sizes in Kconfig:

```kconfig
# For sensor-heavy systems
CONFIG_MEMORY_POOL_SMALL_COUNT=128  # More event objects

# For data logging systems  
CONFIG_MEMORY_POOL_LARGE_COUNT=32   # More data buffers

# For web-heavy systems
CONFIG_MEMORY_POOL_XLARGE_COUNT=16  # More web buffers
```

## Summary

The Memory Pool system provides deterministic, fragmentation-free allocation perfect for 24/7 industrial operation. By following these patterns, your modules will have predictable memory behavior and graceful degradation under memory pressure.

For complete API reference, see `memory_pool.h` and `memory_diagnostics.h`.
