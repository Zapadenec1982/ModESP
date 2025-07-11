# ModuleHeartbeat Documentation

## Overview

ModuleHeartbeat is a critical system component that monitors the health and responsiveness of all registered modules in the ModESP system. It provides automatic detection of unresponsive modules and can attempt to restart them to maintain system stability.

## Features

- **Health Monitoring**: Tracks the last update time of each module
- **Automatic Recovery**: Attempts to restart unresponsive modules
- **Configurable Timeouts**: Different timeout values for different module priorities
- **Statistics Tracking**: Monitors restart counts and system health
- **Resource Efficient**: Minimal memory footprint using static arrays
- **Future Ready**: Prepared for remote sensor support

## Architecture

### Memory Optimization
Instead of dynamic memory allocation, ModuleHeartbeat uses efficient data structures:

```cpp
struct HeartbeatInfo {
    uint32_t last_update_ms;    // 4 bytes
    uint8_t restart_count;      // 1 byte  
    ModuleType type;            // 1 byte
    bool is_active;            // 1 byte
    bool is_remote;            // 1 byte (reserved)
    // Total: 8 bytes per module
};
```

### Integration with ModuleManager

ModuleHeartbeat integrates seamlessly with ModuleManager:
1. Modules are automatically registered during initialization
2. Heartbeats are updated after each successful module update
3. Restart attempts are handled through ModuleManager callbacks

## Configuration

ModuleHeartbeat is configured through the system JSON configuration:

```json
{
    "heartbeat": {
        "enabled": true,
        "check_interval_ms": 5000,      // Check every 5 seconds
        "auto_restart_enabled": true,   // Enable automatic restarts
        "timeouts": {
            "critical_ms": 5000,        // 5 sec for critical modules
            "standard_ms": 30000,       // 30 sec for standard modules  
            "background_ms": 300000     // 5 min for background modules
        }
    }
}
```

### Configuration Parameters

- **enabled**: Enable/disable heartbeat monitoring
- **check_interval_ms**: How often to check module health (milliseconds)
- **auto_restart_enabled**: Whether to automatically restart unresponsive modules
- **timeouts**: Timeout values for different module types

## Module Priorities and Timeouts

| Priority | Default Timeout | Use Case |
|----------|----------------|----------|
| CRITICAL | 5 seconds | Safety systems, watchdog |
| HIGH | 10 seconds | Real-time sensors, control loops |
| STANDARD | 30 seconds | Business logic, normal operations |
| LOW | 60 seconds | UI updates, non-critical features |
| BACKGROUND | 5 minutes | Logging, analytics, maintenance |

## API Reference

### Public Methods

```cpp
// Module registration
void register_module(const char* module_name, ModuleType type);
void unregister_module(const char* module_name);

// Heartbeat updates
void update_heartbeat(const char* module_name);

// Status queries
bool is_module_alive(const char* module_name) const;
uint8_t get_restart_count(const char* module_name) const;
uint8_t get_health_score() const;

// Statistics
const Stats& get_stats() const;
```

## Recovery Strategy

When a module becomes unresponsive:

1. **First Detection**: Log warning, increment restart counter
2. **Restart Attempt 1**: Soft restart (stop() -> init())
3. **Restart Attempt 2**: Full module restart with re-initialization
4. **Restart Attempt 3**: Final attempt before disabling
5. **Module Disabled**: After 3 failed attempts, module is marked inactive

For CRITICAL modules, system restart may be triggered if recovery fails.

## Usage Example

ModuleHeartbeat is automatically integrated with the system:

```cpp
// ModuleHeartbeat is registered automatically in module_registry.cpp
// No manual integration needed

// To check module health from another module:
auto health_score = module_manager->get_module_health_score();

// Module heartbeats are updated automatically in ModuleManager::tick_all()
```

## Monitoring and Diagnostics

### Health Score Calculation
- 100%: All modules responsive
- 80-99%: Minor issues, some modules slow
- 60-79%: Degraded performance, multiple slow modules
- Below 60%: System unhealthy, intervention needed

### Event Bus Integration
ModuleHeartbeat publishes events to the EventBus:

```json
{
    "source": "ModuleHeartbeat",
    "module": "SensorModule",
    "event": "Module unresponsive",
    "timestamp": 1234567890
}
```

## Future Enhancements

### Remote Sensor Support (Planned)
```cpp
// Reserved flags for future use
bool is_remote = false;  // For WiFi/LoRa/RS485 sensors

// Adaptive timeouts for remote modules
if (is_remote && wifi_signal_weak()) {
    timeout *= 2;  // Double timeout for poor connectivity
}
```

### Advanced Features (Roadmap)
- Predictive failure detection
- Module dependency tracking  
- Graceful degradation policies
- Performance trend analysis

## Best Practices

1. **Choose Appropriate Timeouts**: Set realistic timeouts based on module requirements
2. **Monitor Health Score**: Use the system health score for overall system status
3. **Log Analysis**: Review restart patterns to identify problematic modules
4. **Testing**: Simulate module failures during development to verify recovery

## Troubleshooting

| Issue | Possible Cause | Solution |
|-------|---------------|----------|
| Frequent restarts | Timeout too short | Increase timeout for module type |
| Module not restarting | Auto-restart disabled | Enable in configuration |
| High CPU usage | Check interval too short | Increase check_interval_ms |
| Modules marked dead | Module update() blocked | Fix blocking code in module |

## Performance Impact

- Memory: ~8 bytes per module + overhead
- CPU: < 0.1% with 5-second check interval
- Stack: Minimal, no recursive calls

## Conclusion

ModuleHeartbeat provides essential health monitoring for the ModESP system with minimal resource usage. It ensures system reliability through automatic recovery while preparing for future expansion with remote sensors.## Thread Safety and Robustness Improvements

### Mutex Management
- **Proper cleanup**: Destructor now properly deletes the mutex
- **Null checks**: All methods check if mutex is initialized before use
- **Error handling**: Methods return early if mutex is not available

### Code Improvements

1. **Destructor implementation**:
```cpp
~ModuleHeartbeat() {
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}
```

2. **Mutex validation**:
```cpp
if (!m_mutex) {
    ESP_LOGE(TAG, "Mutex not initialized!");
    return ESP_ERR_INVALID_STATE;
}
```

3. **Example timeout-based operation**:
```cpp
bool try_get_module_status(const char* module_name, uint32_t timeout_ms = 100) const {
    if (!module_name || !m_mutex) return false;
    
    if (xSemaphoreTake(m_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
        // Perform operation
        xSemaphoreGive(m_mutex);
        return true;
    }
    ESP_LOGW(TAG, "Failed to acquire mutex within %u ms", timeout_ms);
    return false;
}
```

### When to Use Timeouts

- **Critical operations**: Use `portMAX_DELAY` (current implementation)
  - Module registration/unregistration
  - Heartbeat updates
  - Health checks

- **Non-critical operations**: Use timeout (example provided)
  - Status queries for UI
  - Debugging/diagnostic functions
  - Operations that can fail gracefully

### Benefits

1. **Robustness**: System won't crash if mutex initialization fails
2. **Debugging**: Clear error messages when mutex issues occur
3. **Flexibility**: Example shows how to add timeout-based operations
4. **Resource management**: Proper cleanup prevents memory leaks