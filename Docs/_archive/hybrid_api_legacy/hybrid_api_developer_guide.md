# Hybrid API System Developer Guide

> **Status**: ✅ Complete  
> **Version**: 1.0  
> **For**: Module developers, System integrators, API consumers

## 🎯 Overview

This guide explains how to develop modules and integrate with the Hybrid API Contract system. Whether you're creating new sensor drivers, building UI components, or integrating external systems, this guide provides the knowledge you need.

## 🚀 Quick Start

### **5-Minute Integration**

1. **Create Module Manifest**
```json
{
    "module": {
        "name": "MyModule",
        "version": "1.0.0",
        "description": "Custom module for specialized functionality"
    },
    "rpc_api": {
        "methods": {
            "my_module.get_status": {
                "description": "Get module status",
                "returns": {"status": "string", "uptime": "integer"}
            }
        }
    }
}
```

2. **Implement Module Interface**
```cpp
class MyModule : public BaseModule {
public:
    esp_err_t init() override {
        // Initialize your module
        return ESP_OK;
    }
    
    void register_rpc(IJsonRpcRegistrar* registrar) override {
        registrar->register_method("my_module.get_status",
            [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                result = {
                    {"status", "running"},
                    {"uptime", esp_timer_get_time() / 1000000}
                };
                return ESP_OK;
            }, "Get module status");
    }
};
```

3. **Register in Build System**
```cmake
# In CMakeLists.txt
idf_component_register(
    SRCS "my_module.cpp"
    INCLUDE_DIRS "include"
    REQUIRES core ui
)
```

4. **Test Your Integration**
```bash
# API call example
curl -X POST http://esp32.local/api/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"my_module.get_status","id":1}'
```

## 📋 Development Patterns

### **1. Static API Development**

For APIs that are always available regardless of configuration:

```cpp
// In static_api_registry.cpp
void register_my_static_apis(ApiDispatcher* dispatcher) {
    dispatcher->register_method("system.get_my_info",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            result = {
                {"info", "Always available"},
                {"timestamp", esp_timer_get_time()}
            };
            return ESP_OK;
        }, "Get system information");
}
```

**When to use Static APIs:**
- ✅ System status and control
- ✅ Basic sensor/actuator operations
- ✅ Configuration reading
- ✅ Network connectivity
- ❌ Driver-specific operations
- ❌ Configuration-dependent features

### **2. Dynamic API Development**

For APIs that depend on runtime configuration:

```cpp
// In sensor driver
class CustomSensorDriver : public ISensorDriver {
public:
    nlohmann::json get_ui_schema() const override {
        return {
            {"type", "object"},
            {"title", "Custom Sensor Settings"},
            {"properties", {
                {"sensitivity", {
                    {"type", "integer"},
                    {"minimum", 1},
                    {"maximum", 10},
                    {"default", 5}
                }},
                {"calibration_mode", {
                    {"type", "string"},
                    {"enum", {"auto", "manual"}},
                    {"default", "auto"}
                }}
            }}
        };
    }
};
```

**Automatic API Generation:**
When configured, generates:
- `sensor.{role}.set_sensitivity`
- `sensor.{role}.get_sensitivity`
- `sensor.{role}.set_calibration_mode`
- `sensor.{role}.get_calibration_mode`

**When to use Dynamic APIs:**
- ✅ Sensor-specific operations
- ✅ Driver configuration
- ✅ Hardware-dependent features
- ✅ User-configurable behavior
- ❌ Core system functions
- ❌ Always-available operations

### **3. Configuration Management**

For changes that require restart:

```cpp
// Configuration update API
rpc_methods["config.update_my_settings"] = 
    [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
        // Validate configuration
        if (!validate_my_config(params)) {
            return ESP_ERR_INVALID_ARG;
        }
        
        // Save and mark restart required
        auto& config_mgr = ConfigurationManager::instance();
        esp_err_t ret = config_mgr.update_my_configuration(params);
        
        if (ret == ESP_OK) {
            result = {
                {"success", true},
                {"restart_required", true},
                {"message", "Configuration saved. Restart required to apply."}
            };
        }
        
        return ret;
    };
```

**When to require restart:**
- ✅ Sensor type changes
- ✅ Driver selection changes
- ✅ Hardware configuration changes
- ✅ Module enable/disable
- ❌ Runtime parameter changes
- ❌ User preferences
- ❌ Display settings

## 🏗️ Module Architecture Patterns

### **Pattern 1: Sensor Driver Module**

```cpp
class AdvancedSensorModule : public BaseModule {
private:
    std::map<std::string, std::unique_ptr<ISensorDriver>> drivers_;
    
public:
    esp_err_t init() override {
        // Load sensor configuration
        auto config = load_sensor_config();
        
        // Create drivers based on configuration
        for (const auto& sensor_def : config["sensors"]) {
            std::string role = sensor_def["role"];
            std::string type = sensor_def["type"];
            
            auto driver = SensorDriverRegistry::instance().create_driver(type);
            if (driver) {
                driver->init(get_hal(), sensor_def["config"]);
                drivers_[role] = std::move(driver);
            }
        }
        
        return ESP_OK;
    }
    
    void register_rpc(IJsonRpcRegistrar* registrar) override {
        // Register base APIs
        registrar->register_method("sensor.get_all_readings",
            [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                result = nlohmann::json::object();
                for (const auto& [role, driver] : drivers_) {
                    auto reading = driver->read();
                    result[role] = {
                        {"value", reading.value},
                        {"unit", reading.unit},
                        {"timestamp", reading.timestamp}
                    };
                }
                return ESP_OK;
            });
        
        // Register driver-specific APIs (dynamic)
        for (const auto& [role, driver] : drivers_) {
            register_driver_apis(registrar, role, driver.get());
        }
    }
    
private:
    void register_driver_apis(IJsonRpcRegistrar* registrar, 
                             const std::string& role,
                             ISensorDriver* driver) {
        std::string base = "sensor." + role + ".";
        
        // Get value API
        registrar->register_method(base + "get_value",
            [driver](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                auto reading = driver->read();
                result = {
                    {"value", reading.value},
                    {"unit", reading.unit},
                    {"timestamp", reading.timestamp}
                };
                return ESP_OK;
            });
        
        // Configuration APIs based on driver schema
        auto schema = driver->get_ui_schema();
        register_schema_apis(registrar, base, schema, driver);
    }
};
```

### **Pattern 2: Control Module**

```cpp
class ClimateControlModule : public BaseModule {
private:
    float setpoint_ = 20.0f;
    std::string mode_ = "auto";
    
public:
    void register_rpc(IJsonRpcRegistrar* registrar) override {
        // Setpoint control
        registrar->register_method("climate.set_setpoint",
            [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                if (!params.contains("value")) {
                    return ESP_ERR_INVALID_ARG;
                }
                
                float new_setpoint = params["value"];
                if (new_setpoint < -40.0f || new_setpoint > 60.0f) {
                    return ESP_ERR_INVALID_ARG;
                }
                
                float old_setpoint = setpoint_;
                setpoint_ = new_setpoint;
                
                // Update SharedState
                SharedState::set(State::ClimateSetpoint, setpoint_);
                
                // Publish change event
                EventBus::publish(Event::ClimateSetpointChanged, {
                    {"old", old_setpoint},
                    {"new", new_setpoint},
                    {"source", "api"}
                });
                
                result = {
                    {"success", true},
                    {"setpoint", setpoint_}
                };
                return ESP_OK;
            });
        
        // Mode control
        registrar->register_method("climate.set_mode",
            [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                if (!params.contains("mode")) {
                    return ESP_ERR_INVALID_ARG;
                }
                
                std::string new_mode = params["mode"];
                if (new_mode != "auto" && new_mode != "manual" && new_mode != "off") {
                    return ESP_ERR_INVALID_ARG;
                }
                
                std::string old_mode = mode_;
                mode_ = new_mode;
                
                SharedState::set(State::ClimateMode, mode_);
                
                EventBus::publish(Event::ClimateModeChanged, {
                    {"old", old_mode},
                    {"new", new_mode}
                });
                
                result = {
                    {"success", true},
                    {"mode", mode_}
                };
                return ESP_OK;
            });
    }
    
    void update() override {
        // Control logic based on setpoint and mode
        if (mode_ == "auto") {
            perform_automatic_control();
        }
    }
};
```

### **Pattern 3: Service Module**

```cpp
class DataLoggerModule : public BaseModule {
private:
    std::queue<LogEntry> log_queue_;
    
public:
    void register_rpc(IJsonRpcRegistrar* registrar) override {
        // Get logs API
        registrar->register_method("logger.get_logs",
            [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                int limit = params.value("limit", 100);
                
                nlohmann::json logs = nlohmann::json::array();
                int count = 0;
                
                auto queue_copy = log_queue_;
                while (!queue_copy.empty() && count < limit) {
                    const auto& entry = queue_copy.front();
                    logs.push_back({
                        {"timestamp", entry.timestamp},
                        {"level", entry.level},
                        {"message", entry.message}
                    });
                    queue_copy.pop();
                    count++;
                }
                
                result = {
                    {"logs", logs},
                    {"total", log_queue_.size()}
                };
                return ESP_OK;
            });
        
        // Clear logs API
        registrar->register_method("logger.clear_logs",
            [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
                while (!log_queue_.empty()) {
                    log_queue_.pop();
                }
                
                result = {
                    {"success", true},
                    {"message", "Logs cleared"}
                };
                return ESP_OK;
            });
    }
    
    void update() override {
        // Process log entries
        process_log_queue();
    }
};
```

## 🔧 Configuration Management

### **Configuration File Structure**
```json
{
    "modules": {
        "my_module": {
            "enabled": true,
            "config": {
                "parameter1": "value1",
                "parameter2": 42
            }
        }
    }
}
```

### **Configuration Loading**
```cpp
nlohmann::json load_module_config() {
    // Try NVS first
    auto config = load_config_from_nvs("my_module");
    if (!config.empty()) {
        return config;
    }
    
    // Fall back to default configuration
    return {
        {"parameter1", "default_value"},
        {"parameter2", 10}
    };
}
```

### **Configuration Validation**
```cpp
esp_err_t validate_module_config(const nlohmann::json& config) {
    // Required fields
    if (!config.contains("parameter1")) {
        ESP_LOGE(TAG, "Missing required parameter: parameter1");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Type validation
    if (!config["parameter1"].is_string()) {
        ESP_LOGE(TAG, "parameter1 must be a string");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Range validation
    if (config.contains("parameter2")) {
        int value = config["parameter2"];
        if (value < 1 || value > 100) {
            ESP_LOGE(TAG, "parameter2 must be between 1 and 100");
            return ESP_ERR_INVALID_ARG;
        }
    }
    
    return ESP_OK;
}
```

## 🎨 UI Integration

### **Automatic UI Generation**
Based on your module manifest, UI controls are automatically generated:

```json
{
    "ui_interfaces": {
        "web": {
            "controls": [
                {
                    "id": "my_parameter",
                    "type": "slider",
                    "label": "My Parameter",
                    "min": 1,
                    "max": 100,
                    "read_method": "my_module.get_parameter",
                    "write_method": "my_module.set_parameter"
                },
                {
                    "id": "my_mode",
                    "type": "select",
                    "label": "Operation Mode",
                    "options": ["auto", "manual", "off"],
                    "read_method": "my_module.get_mode",
                    "write_method": "my_module.set_mode"
                }
            ]
        },
        "mqtt": {
            "telemetry": {
                "my_status": {
                    "topic": "modesp/my_module/status",
                    "source": "state.my_module.status",
                    "interval": 30
                }
            }
        }
    }
}
```

### **Custom UI Components**
```cpp
// Generate custom UI schema
nlohmann::json get_custom_ui_schema() {
    return {
        {"type", "object"},
        {"title", "Advanced Settings"},
        {"properties", {
            {"advanced_mode", {
                {"type", "boolean"},
                {"title", "Enable Advanced Mode"},
                {"description", "Enables expert-level controls"}
            }},
            {"expert_settings", {
                {"type", "object"},
                {"title", "Expert Settings"},
                {"condition", "advanced_mode == true"},  // Conditional visibility
                {"properties", {
                    {"debug_level", {
                        {"type", "integer"},
                        {"minimum", 0},
                        {"maximum", 5}
                    }}
                }}
            }}
        }}
    };
}
```

## 🧪 Testing Strategies

### **Unit Testing**
```cpp
// test_my_module.cpp
#include "unity.h"
#include "my_module.h"

void setUp() {
    // Setup test environment
}

void tearDown() {
    // Cleanup after test
}

void test_module_initialization() {
    MyModule module;
    
    esp_err_t ret = module.init();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Test that module is properly initialized
    TEST_ASSERT_TRUE(module.is_initialized());
}

void test_api_endpoints() {
    MyModule module;
    module.init();
    
    ApiDispatcher dispatcher;
    module.register_rpc(&dispatcher);
    
    // Test API call
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"method", "my_module.get_status"},
        {"id", 1}
    };
    
    nlohmann::json response;
    esp_err_t ret = dispatcher.execute_json_rpc(request, response);
    
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(response.contains("result"));
}

void test_configuration_validation() {
    nlohmann::json valid_config = {
        {"parameter1", "test_value"},
        {"parameter2", 50}
    };
    
    esp_err_t ret = validate_module_config(valid_config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    nlohmann::json invalid_config = {
        {"parameter2", 150}  // Out of range
    };
    
    ret = validate_module_config(invalid_config);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}
```

### **Integration Testing**
```cpp
void test_full_module_integration() {
    // Initialize core systems
    EventBus::init();
    SharedState::init();
    
    // Initialize module
    MyModule module;
    module.init();
    
    // Test SharedState integration
    SharedState::set("my_module.test_value", 42);
    
    // Test EventBus integration
    bool event_received = false;
    EventBus::subscribe("my_module.test_event", 
        [&event_received](const EventBus::Event& event) {
            event_received = true;
        });
    
    module.trigger_test_event();
    
    // Allow event processing
    vTaskDelay(pdMS_TO_TICKS(100));
    
    TEST_ASSERT_TRUE(event_received);
}
```

### **API Testing**
```bash
#!/bin/bash
# api_test.sh

ESP32_IP="192.168.1.100"
BASE_URL="http://${ESP32_IP}/api/jsonrpc"

# Test basic API call
echo "Testing basic API call..."
curl -s -X POST ${BASE_URL} \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"system.get_status","id":1}' | jq .

# Test module-specific API
echo "Testing module API..."
curl -s -X POST ${BASE_URL} \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"my_module.get_status","id":2}' | jq .

# Test configuration update
echo "Testing configuration update..."
curl -s -X POST ${BASE_URL} \
  -H "Content-Type: application/json" \
  -d '{
    "jsonrpc":"2.0",
    "method":"config.update_my_module",
    "params":{"parameter1":"new_value","parameter2":75},
    "id":3
  }' | jq .
```

## 🔍 Debugging and Diagnostics

### **Logging Best Practices**
```cpp
static const char* TAG = "MyModule";

void MyModule::debug_operation() {
    ESP_LOGD(TAG, "Debug: Starting operation with parameter: %d", parameter_);
    ESP_LOGI(TAG, "Info: Operation completed successfully");
    ESP_LOGW(TAG, "Warning: Parameter value near upper limit: %d", parameter_);
    ESP_LOGE(TAG, "Error: Operation failed with code: %d", error_code);
}
```

### **Performance Monitoring**
```cpp
class PerformanceMonitor {
public:
    static void start_operation(const std::string& operation) {
        start_times_[operation] = esp_timer_get_time();
    }
    
    static void end_operation(const std::string& operation) {
        auto start_time = start_times_[operation];
        auto duration_us = esp_timer_get_time() - start_time;
        
        ESP_LOGI("Performance", "%s took %lld microseconds", 
                 operation.c_str(), duration_us);
        
        // Update performance metrics
        update_performance_metrics(operation, duration_us);
    }
    
private:
    static std::map<std::string, uint64_t> start_times_;
};

// Usage
void MyModule::expensive_operation() {
    PerformanceMonitor::start_operation("expensive_operation");
    
    // Your operation here
    
    PerformanceMonitor::end_operation("expensive_operation");
}
```

### **Memory Monitoring**
```cpp
void check_memory_usage() {
    size_t free_heap = esp_get_free_heap_size();
    size_t min_free_heap = esp_get_minimum_free_heap_size();
    
    ESP_LOGI(TAG, "Free heap: %zu bytes", free_heap);
    ESP_LOGI(TAG, "Minimum free heap: %zu bytes", min_free_heap);
    
    if (free_heap < 10000) {
        ESP_LOGW(TAG, "Low memory warning: %zu bytes free", free_heap);
    }
}
```

## 🚀 Best Practices

### **Code Organization**
```
my_module/
├── include/
│   ├── my_module.h
│   └── my_module_config.h
├── src/
│   ├── my_module.cpp
│   └── my_module_api.cpp
├── test/
│   ├── test_my_module.cpp
│   └── test_data/
├── manifests/
│   └── my_module_manifest.json
├── CMakeLists.txt
└── README.md
```

### **Error Handling**
```cpp
esp_err_t MyModule::risky_operation() {
    // Validate inputs
    if (!is_initialized()) {
        ESP_LOGE(TAG, "Module not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Perform operation with error checking
    esp_err_t ret = perform_hardware_operation();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Hardware operation failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Update state only on success
    update_internal_state();
    
    return ESP_OK;
}
```

### **Resource Management**
```cpp
class ResourceManager {
private:
    std::unique_ptr<HardwareResource> resource_;
    
public:
    esp_err_t init() {
        resource_ = std::make_unique<HardwareResource>();
        
        esp_err_t ret = resource_->initialize();
        if (ret != ESP_OK) {
            resource_.reset();  // Clean up on failure
            return ret;
        }
        
        return ESP_OK;
    }
    
    ~ResourceManager() {
        if (resource_) {
            resource_->cleanup();  // RAII cleanup
        }
    }
};
```

### **Thread Safety**
```cpp
class ThreadSafeModule {
private:
    mutable std::mutex mutex_;
    volatile bool running_ = false;
    
public:
    void set_running(bool running) {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = running;
    }
    
    bool is_running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }
};
```

## 📈 Performance Optimization

### **Memory Optimization**
```cpp
// Use object pools for frequent allocations
class ObjectPool {
public:
    template<typename T>
    std::unique_ptr<T> acquire() {
        if (!pool_.empty()) {
            auto obj = std::move(pool_.back());
            pool_.pop_back();
            return obj;
        }
        return std::make_unique<T>();
    }
    
    template<typename T>
    void release(std::unique_ptr<T> obj) {
        obj->reset();  // Clear object state
        pool_.push_back(std::move(obj));
    }
    
private:
    std::vector<std::unique_ptr<void>> pool_;
};
```

### **CPU Optimization**
```cpp
// Use efficient algorithms and data structures
class OptimizedLookup {
private:
    std::unordered_map<std::string, int> lookup_table_;
    
public:
    // O(1) lookup instead of O(n) search
    int find_value(const std::string& key) const {
        auto it = lookup_table_.find(key);
        return (it != lookup_table_.end()) ? it->second : -1;
    }
};
```

## 📚 Additional Resources

### **Documentation Links**
- [Module Manifest Schema Reference](../05_UI_API/manifest_schema.md)
- [API Contract Reference](../02_Architecture/api_contracts.md)
- [SharedState Usage Guide](../03_Components/core/shared_state.md)
- [EventBus Integration Guide](../03_Components/core/event_bus.md)

### **Example Modules**
- [Simple Sensor Module Example](../examples/simple_sensor_module/)
- [Advanced Control Module Example](../examples/advanced_control_module/)
- [Configuration Management Example](../examples/configuration_module/)

### **Tools and Utilities**
- [Manifest Validator Tool](../../tools/validate_manifest.py)
- [API Testing Scripts](../../tools/api_test_suite/)
- [Performance Profiling Tools](../../tools/performance_monitor/)

---

## ✅ Developer Checklist

When developing a new module:

- [ ] Module manifest created and validated
- [ ] Module interface implemented correctly
- [ ] RPC methods registered properly
- [ ] Configuration validation implemented
- [ ] Unit tests written and passing
- [ ] Integration tests verified
- [ ] Memory usage profiled
- [ ] Performance benchmarked
- [ ] Documentation updated
- [ ] Code review completed

The Hybrid API system provides a powerful foundation for building modular, configurable, and maintainable embedded systems. Follow these patterns and best practices to create robust modules that integrate seamlessly with the platform.
