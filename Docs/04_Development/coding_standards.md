# ModuChill Development Guidelines

## 🎯 Coding Standards & Best Practices

### C++ Code Style Guidelines

#### Naming Conventions
```cpp
// Classes: PascalCase
class SensorModule {};
class ModuleHeartbeat {};

// Functions/Methods: snake_case
void initialize_system();
bool process_sensor_data();

// Variables: snake_case
float temperature_celsius;
uint32_t heartbeat_interval_ms;

// Constants: UPPER_SNAKE_CASE
static constexpr uint32_t DEFAULT_TIMEOUT_MS = 5000;
static constexpr size_t MAX_SENSORS = 64;

// Private members: trailing underscore
class MyClass {
private:
    int private_value_;
    std::string internal_state_;
};

// Enums: PascalCase with descriptive prefix
enum class SensorType {
    DS18B20,
    NTC_THERMISTOR,
    PRESSURE_4_20MA,
    GPIO_INPUT
};
```

#### File Organization
```
component_name/
├── include/
│   ├── component_name.h          # Main public header
│   └── internal/
│       └── private_header.h      # Internal headers
├── src/
│   ├── component_name.cpp        # Main implementation
│   ├── submodule1.cpp           # Additional implementation files
│   └── private_implementation.cpp
├── test/
│   ├── test_component.cpp        # Unit tests
│   └── integration_test.cpp      # Integration tests
├── CMakeLists.txt                # Build configuration
└── README.md                     # Component documentation
```

#### Header Structure
```cpp
#pragma once

// System includes first
#include <memory>
#include <string>
#include <vector>

// ESP-IDF includes
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Project includes
#include "core/shared_state.h"
#include "hal/interfaces.h"

namespace moduchil {

/**
 * @brief Brief description of the class
 * 
 * Detailed description explaining the purpose,
 * usage patterns, and important considerations.
 * 
 * Example usage:
 * @code
 * MyClass instance;
 * instance.initialize();
 * @endcode
 */
class MyClass {
public:
    /**
     * @brief Constructor with parameters
     * @param param Description of parameter
     */
    explicit MyClass(const std::string& param);
    
    /**
     * @brief Initialize the component
     * @return true if successful, false otherwise
     */
    bool initialize();
    
private:
    std::string config_;
    static constexpr const char* TAG = "MyClass";
};

} // namespace moduchil
```
### Memory Management Rules

#### 1. Static Allocation Preferred
```cpp
// ✅ Good: Static allocation
static SensorModule sensor_module;
static ActuatorModule actuator_module;

// ⚠️ Avoid: Dynamic allocation in runtime
// std::unique_ptr<SensorModule> sensor_module = std::make_unique<SensorModule>();
```

#### 2. RAII Pattern
```cpp
class ResourceManager {
public:
    ResourceManager() {
        // Acquire resources in constructor
        gpio_config(&config);
    }
    
    ~ResourceManager() {
        // Release resources in destructor
        gpio_reset_pin(pin_number);
    }
    
    // Delete copy constructor and assignment operator
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
};
```

#### 3. Error Handling
```cpp
// ✅ Good: Return optional or error codes
std::optional<float> read_temperature() {
    float value;
    if (sensor_read(&value) == ESP_OK) {
        return value;
    }
    return std::nullopt;
}

// ✅ Good: Use ESP_ERROR_CHECK for critical operations
void initialize_hardware() {
    ESP_ERROR_CHECK(gpio_config(&config));
    ESP_ERROR_CHECK(uart_driver_install(uart_port, 1024, 0, 0, nullptr, 0));
}

// ❌ Avoid: Throwing exceptions (disabled in project)
// throw std::runtime_error("Sensor failed");
```

### Threading and Concurrency

#### Task Creation Guidelines
```cpp
void create_sensor_task() {
    xTaskCreatePinnedToCore(
        sensor_task_impl,           // Task function
        "sensor_task",              // Task name
        4096,                       // Stack size (4KB)
        nullptr,                    // Parameters
        tskIDLE_PRIORITY + 2,       // Priority (2 = normal sensors)
        &sensor_task_handle,        // Task handle
        1                           // Core 1 (app core)
    );
}
```

#### Priority Guidelines
```
Core 0 (Protocol CPU):
├── WiFi Stack (managed by ESP-IDF)
├── Bluetooth (if enabled)
└── Network tasks

Core 1 (Application CPU):
├── Priority 20+: Critical safety tasks
├── Priority 10-19: Real-time control
├── Priority 5-9: Sensor/actuator I/O
├── Priority 2-4: Business logic
├── Priority 1: Background tasks
└── Priority 0: Idle tasks
```

#### Synchronization
```cpp
// ✅ Use FreeRTOS primitives
static SemaphoreHandle_t data_mutex = nullptr;

void initialize_sync() {
    data_mutex = xSemaphoreCreateMutex();
    assert(data_mutex != nullptr);
}

bool update_shared_data(const Data& new_data) {
    if (xSemaphoreTake(data_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        shared_data = new_data;
        xSemaphoreGive(data_mutex);
        return true;
    }
    return false;
}
```

### Logging Guidelines

#### Log Levels
```cpp
// ERROR: System errors, failed operations
ESP_LOGE(TAG, "Failed to initialize sensor: %s", esp_err_to_name(err));

// WARN: Non-fatal issues, degraded performance
ESP_LOGW(TAG, "Sensor reading out of range: %.2f", value);

// INFO: Important system events, state changes
ESP_LOGI(TAG, "System initialized successfully");

// DEBUG: Detailed debugging information
ESP_LOGD(TAG, "Processing sensor data: id=%s, value=%.2f", id.c_str(), value);

// VERBOSE: Very detailed debugging (usually disabled)
ESP_LOGV(TAG, "Raw ADC reading: %d", adc_raw);
```

#### Structured Logging for Metrics
```cpp
// Performance metrics
#define LOG_PERFORMANCE(operation, duration_us) \
    ESP_LOGI("PERF", "METRIC|%s|%llu", operation, duration_us)

// System metrics
#define LOG_SYSTEM_METRIC(metric, value) \
    ESP_LOGI("METRICS", "SYSTEM|%s|%.2f|%llu", metric, value, esp_timer_get_time())

// Usage examples
LOG_PERFORMANCE("sensor_read", 1250);
LOG_SYSTEM_METRIC("free_heap", esp_get_free_heap_size());
```

### Testing Standards

#### Unit Test Structure
```cpp
#include "unity.h"
#include "my_component.h"

void setUp(void) {
    // Setup before each test
}

void tearDown(void) {
    // Cleanup after each test
}

TEST_CASE("Component initialization", "[my_component]") {
    MyComponent component;
    TEST_ASSERT_TRUE(component.initialize());
    TEST_ASSERT_EQUAL(ComponentState::INITIALIZED, component.get_state());
}

TEST_CASE("Error handling", "[my_component]") {
    MyComponent component;
    // Test error conditions
    TEST_ASSERT_FALSE(component.process_invalid_data());
}
```

#### Integration Test Guidelines
```cpp
TEST_CASE("Sensor to actuator pipeline", "[integration]") {
    // 1. Setup complete system
    SensorModule sensors;
    ActuatorModule actuators;
    SharedState state;
    
    // 2. Initialize all components
    TEST_ASSERT_TRUE(sensors.initialize());
    TEST_ASSERT_TRUE(actuators.initialize());
    
    // 3. Simulate sensor input
    sensors.simulate_temperature_reading(25.5f);
    
    // 4. Wait for processing
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 5. Verify actuator response
    auto temp = state.get<float>("state.sensor.temperature");
    TEST_ASSERT_TRUE(temp.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 25.5f, temp.value());
}
```
### Performance Guidelines

#### Timing Constraints
```cpp
// Critical timing requirements
constexpr uint32_t MAX_SENSOR_READ_TIME_US = 10000;      // 10ms
constexpr uint32_t MAX_ACTUATOR_RESPONSE_TIME_US = 5000;  // 5ms
constexpr uint32_t MAX_EVENT_PROCESSING_TIME_US = 1000;   // 1ms

// Performance monitoring
class PerformanceTimer {
public:
    explicit PerformanceTimer(const char* operation) 
        : operation_(operation), start_time_(esp_timer_get_time()) {}
    
    ~PerformanceTimer() {
        uint64_t duration = esp_timer_get_time() - start_time_;
        if (duration > warning_threshold_) {
            ESP_LOGW("PERF", "%s took %llu μs (warning threshold: %llu μs)", 
                     operation_, duration, warning_threshold_);
        }
        LOG_PERFORMANCE(operation_, duration);
    }
    
private:
    const char* operation_;
    uint64_t start_time_;
    static constexpr uint64_t warning_threshold_ = 10000; // 10ms
};

#define PERF_TIMER(name) PerformanceTimer _timer(name)
```

#### Memory Optimization
```cpp
// ✅ Use packed structures for network/storage
struct __attribute__((packed)) SensorReading {
    uint16_t sensor_id;
    float value;
    uint32_t timestamp;
};

// ✅ Use string_view for read-only string operations
void process_config(std::string_view config_json);

// ✅ Use stack allocation for temporary objects
void process_data() {
    std::array<float, 10> temp_buffer;  // Stack allocation
    // Process data...
}
```

### Configuration Management

#### JSON Schema Validation
```cpp
// Define schemas for validation
const json sensor_schema = {
    {"type", "object"},
    {"required", {"role", "type", "config"}},
    {"properties", {
        {"role", {"type", "string", "minLength", 1}},
        {"type", {"type", "string", "enum", {"DS18B20", "NTC", "GPIO_INPUT"}}},
        {"config", {"type", "object"}}
    }}
};

bool validate_sensor_config(const json& config) {
    // Use nlohmann::json validation
    try {
        // Validation logic here
        return true;
    } catch (const std::exception& e) {
        ESP_LOGE(TAG, "Config validation failed: %s", e.what());
        return false;
    }
}
```

#### Configuration Hot-Reload
```cpp
class ConfigWatcher {
public:
    void start_watching() {
        xTaskCreate(watch_task, "config_watch", 2048, this, 1, nullptr);
    }
    
private:
    static void watch_task(void* param) {
        auto* watcher = static_cast<ConfigWatcher*>(param);
        while (true) {
            if (watcher->check_config_changed()) {
                watcher->reload_configuration();
            }
            vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
        }
    }
};
```

### Security Guidelines

#### Input Validation
```cpp
bool validate_api_input(const json& input) {
    // 1. Check required fields
    if (!input.contains("type") || !input.contains("value")) {
        return false;
    }
    
    // 2. Validate data types
    if (!input["value"].is_number()) {
        return false;
    }
    
    // 3. Check ranges
    float value = input["value"];
    if (value < MIN_ALLOWED_VALUE || value > MAX_ALLOWED_VALUE) {
        return false;
    }
    
    return true;
}
```

#### Secure Communications
```cpp
// ✅ Always use TLS for external communications
esp_mqtt_client_config_t mqtt_cfg = {
    .uri = "mqtts://broker.example.com:8883",
    .cert_pem = server_cert_pem_start,
    .client_cert_pem = client_cert_pem_start,
    .client_key_pem = client_key_pem_start,
};
```

### Documentation Requirements

#### API Documentation
```cpp
/**
 * @brief Read temperature from specified sensor
 * 
 * Reads the current temperature value from the sensor identified
 * by sensor_id. The function will block for up to timeout_ms
 * milliseconds waiting for a valid reading.
 * 
 * @param sensor_id Unique identifier for the sensor
 * @param timeout_ms Maximum time to wait for reading (milliseconds)
 * 
 * @return Temperature value in Celsius if successful, std::nullopt if failed
 * 
 * @note This function is thread-safe
 * @warning Sensor must be initialized before calling this function
 * 
 * @example
 * @code
 * auto temp = read_temperature("chamber_temp", 1000);
 * if (temp.has_value()) {
 *     ESP_LOGI(TAG, "Temperature: %.2f°C", temp.value());
 * }
 * @endcode
 */
std::optional<float> read_temperature(const std::string& sensor_id, 
                                     uint32_t timeout_ms = 1000);
```

#### Module README Template
```markdown
# ModuleName

## Overview
Brief description of module purpose and functionality.

## Features
- Feature 1
- Feature 2
- Feature 3

## API Reference
Link to detailed API documentation.

## Configuration
Example configuration with explanations.

## Examples
Basic usage examples.

## Testing
How to run tests for this module.

## Dependencies
List of dependencies and requirements.
```

This guidelines document ensures consistent, maintainable, and high-quality code across the ModuChill project while leveraging AI-assisted development effectively.