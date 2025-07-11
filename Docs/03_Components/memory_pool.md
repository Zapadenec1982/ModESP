# Memory Pool System Documentation

## Overview

The Memory Pool system provides deterministic, fragmentation-free memory allocation for ModESP's industrial refrigeration control system. It's designed for 24/7 operation with predictable performance and zero heap fragmentation.

## Features

- **O(1) Lock-free Allocation**: Constant time allocation/deallocation using atomic operations
- **Zero Fragmentation**: Fixed-size blocks eliminate heap fragmentation
- **Thread-Safe**: Lock-free implementation safe for multi-threaded use
- **Comprehensive Diagnostics**: Real-time monitoring and statistics
- **RAII Support**: Automatic memory management with PooledPtr
- **Industrial Grade**: Suitable for safety-critical applications (IEC 61508 compliant design)

## Architecture

### Pool Sizes

The system provides 5 pool sizes optimized for typical embedded use cases:

| Pool Name | Block Size | Default Count | Typical Usage |
|-----------|------------|---------------|---------------|
| TINY      | 32 bytes   | 128 blocks    | Event headers, small commands |
| SMALL     | 64 bytes   | 64 blocks     | Standard Events with data |
| MEDIUM    | 128 bytes  | 32 blocks     | JSON payloads, control messages |
| LARGE     | 256 bytes  | 16 blocks     | Sensor data batches |
| XLARGE    | 512 bytes  | 8 blocks      | Web responses, config blocks |

### Memory Layout

```
┌─────────────────────────────────────────────┐
│            Memory Pool Manager              │
├─────────────────────────────────────────────┤
│ ┌─────────┐ ┌─────────┐ ┌─────────┐       │
│ │  TINY   │ │  SMALL  │ │ MEDIUM  │  ...  │
│ │ 32B×128 │ │ 64B×64  │ │128B×32  │       │
│ └─────────┘ └─────────┘ └─────────┘       │
├─────────────────────────────────────────────┤
│          Free List (Lock-free Stack)        │
│    Head → [3] → [7] → [1] → INVALID        │
└─────────────────────────────────────────────┘
```

## Usage

### Basic Allocation

```cpp
#include "memory_pool.h"

// Get pool manager instance
auto& pool_manager = ModESP::Memory::get_pool_manager();

// Allocate memory
void* ptr = pool_manager.allocate(100); // Automatically selects MEDIUM pool
if (ptr) {
    // Use memory
    MyStruct* obj = new(ptr) MyStruct();
    
    // Cleanup
    obj->~MyStruct();
    pool_manager.deallocate(ptr, 100);
}
```

### RAII with PooledPtr

```cpp
// Automatic memory management
auto sensor_data = ModESP::make_pooled<SensorData>();
if (sensor_data) {
    sensor_data->temperature = 25.5f;
    sensor_data->humidity = 60.0f;
    // Memory automatically returned when sensor_data goes out of scope
}
```

### Pooled Events

```cpp
#include "pooled_event.h"

// Create event with pooled memory
auto event = ModESP::PooledEvent::create(
    "sensor.temperature.updated",
    {{"value", 25.5}, {"unit", "C"}},
    EventBus::Priority::NORMAL
);

// Or use convenience function
ModESP::PooledEventBus::publish_pooled(
    "system.status",
    {{"status", "ready"}},
    EventBus::Priority::HIGH
);
```

### Diagnostics

```cpp
#include "memory_diagnostics.h"

// Create diagnostics instance
ModESP::Memory::MemoryDiagnostics diag(pool_manager);

// Get current statistics
auto stats = diag.get_diagnostics();
ESP_LOGI(TAG, "Memory usage: %d%%", stats.overall_utilization);
ESP_LOGI(TAG, "Fragmentation: %d%%", stats.fragmentation_index);

// Log detailed report
diag.log_stats();
```

### Memory Tracking

```cpp
// Track memory usage for operations
{
    TRACK_MEMORY_USAGE("Complex Algorithm");
    // Your code here...
    // Automatically logs memory delta on scope exit
}
```

## Configuration (Kconfig)

Configure via `idf.py menuconfig` → "Memory Pool Configuration":

```
CONFIG_MEMORY_POOL_ENABLE=y              # Enable system
CONFIG_MEMORY_POOL_STRICT_MODE=y         # No heap fallback (production)
CONFIG_MEMORY_POOL_DIAGNOSTICS=y         # Enable diagnostics
CONFIG_MEMORY_POOL_TINY_COUNT=128        # Number of 32B blocks
CONFIG_MEMORY_POOL_SMALL_COUNT=64        # Number of 64B blocks
CONFIG_MEMORY_POOL_MEDIUM_COUNT=32       # Number of 128B blocks
CONFIG_MEMORY_POOL_LARGE_COUNT=16        # Number of 256B blocks
CONFIG_MEMORY_POOL_XLARGE_COUNT=8        # Number of 512B blocks
CONFIG_MEMORY_POOL_LOW_THRESHOLD=80      # Low memory warning (%)
CONFIG_MEMORY_POOL_CRITICAL_THRESHOLD=95 # Critical alert (%)
```

## Best Practices

### 1. Size Selection
- Choose the smallest pool that fits your data
- Consider alignment and padding
- Group related small allocations

### 2. Lifetime Management
- Prefer RAII (PooledPtr) over manual management
- Keep allocations short-lived
- Avoid holding pool memory across long operations

### 3. Error Handling
```cpp
// Always check allocation results
auto data = ModESP::make_pooled<LargeData>();
if (!data) {
    // Handle allocation failure
    // - Log error
    // - Degrade gracefully
    // - Trigger memory pressure handling
}
```

### 4. Production Configuration
- Enable `CONFIG_MEMORY_POOL_STRICT_MODE`
- Disable `CONFIG_MEMORY_POOL_POISON_FREED`
- Set appropriate alert thresholds
- Monitor fragmentation index

## Performance Characteristics

| Operation | Time Complexity | Typical Time |
|-----------|----------------|--------------|
| Allocate  | O(1)           | < 100 cycles |
| Deallocate| O(1)           | < 50 cycles  |
| Get Stats | O(1)           | < 200 cycles |

## Memory Overhead

- Metadata: ~5% of total pool size
- Alignment: 8-byte boundaries
- No hidden allocations
- Predictable memory footprint

## Integration with ModESP

### EventBus Integration
The EventBus automatically uses pooled allocation when available:
```cpp
// Automatic pooled allocation
EventBus::publish("sensor.update", data);
```

### Module Integration
Modules can allocate from their quota:
```cpp
class MyModule : public IModule {
protected:
    void init() override {
        // Allocate module-specific memory
        auto buffer = allocate_pooled<Buffer>();
    }
};
```

## Troubleshooting

### Common Issues

1. **Pool Exhaustion**
   - Check pool sizes in Kconfig
   - Review allocation patterns
   - Enable diagnostics to identify heavy users

2. **Fragmentation**
   - Monitor fragmentation index
   - Ensure proper deallocation
   - Check for memory leaks

3. **Performance**
   - Verify lock-free operation
   - Check for allocation hot spots
   - Use benchmarks to validate

### Debug Tools

```cpp
// Enable allocation tracking
#define CONFIG_MEMORY_POOL_TRACE_ALLOCATIONS

// Run benchmarks
auto results = MemoryPoolBenchmark::run_all_benchmarks();

// Detect potential leaks
auto leaks = diag.detect_potential_leaks(60000); // 60s threshold
```

## Safety Considerations

1. **No Double-Free**: Pool validates all deallocations
2. **Bounds Checking**: Pointer validation on deallocation
3. **Watchdog Integration**: Critical alerts trigger system response
4. **Emergency Reserve**: Optional reserve for critical operations

## Future Enhancements

- [ ] Dynamic pool resizing
- [ ] Per-module quotas
- [ ] Advanced leak detection
- [ ] Integration with FreeRTOS heap
- [ ] Remote diagnostics via MQTT

## Example Projects

See `/examples/memory_pool_example.cpp` for complete usage examples.
