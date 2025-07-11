# UI/API Technical Reference

## Implementation Details

### Compile-time Generation Process

#### 1. Schema Discovery
```python
# ui_generator.py scans for schemas
components/
  module1/ui_schema.json
  module2/ui_schema.json
  ...
```

#### 2. Generated Files Structure
```cpp
// web_ui_generated.h
const char INDEX_HTML[] PROGMEM = R"(...)";  // Complete HTML
const UIElementMap UI_ELEMENT_MAP[] PROGMEM = {
    {"module1_temp", "module1.get_temp", "module1.temperature"},
    // ...
};

// mqtt_topics_generated.h  
#define MQTT_TOPIC_MODULE1_TEMP "modesp/module1/temperature"
const MqttTelemetryMap MQTT_TELEMETRY_MAP[] PROGMEM = {
    {MQTT_TOPIC_MODULE1_TEMP, "state.module1.temperature", 60},
    // ...
};

// lcd_menu_generated.h
const MenuItem MAIN_MENU[] PROGMEM = {
    {"Module 1", MENU_TYPE_SUBMENU, {.submenu_id = 1}},
    // ...
};
```

### Memory Layout

#### PROGMEM Usage
```
Flash Memory Map:
┌─────────────────────┐ 0x3F400000
│   Application Code   │
├─────────────────────┤
│  Generated UI Data   │ <- INDEX_HTML, menus, mappings
├─────────────────────┤
│    String Tables     │ <- UI labels, method names  
├─────────────────────┤
│   Constant Data      │
└─────────────────────┘
```

#### RAM Usage (Runtime)
```
Heap:
┌─────────────────────┐
│   Module Instances   │ <- Business logic objects
├─────────────────────┤
│   Dynamic UI State   │ <- Current values only
├─────────────────────┤
│  Request Buffers     │ <- Reusable, ~2KB
└─────────────────────┘
```

### Communication Flow

#### Web UI Request
```
1. Browser → GET /api/data
2. WebUIModule::handle_api_request()
3. For each element in UI_ELEMENT_MAP:
   a. Check SharedState for value
   b. If not found, call RPC method
   c. Add to JSON response
4. Send JSON → Browser
5. JavaScript updates DOM
```

#### MQTT Publishing
```
1. Timer tick (based on interval)
2. MQTTUIAdapter::publish_telemetry()
3. For each item in MQTT_TELEMETRY_MAP:
   a. Get value from SharedState
   b. Publish to MQTT topic
4. Reset timer
```

### RPC Method Resolution

#### Registration Phase
```cpp
// During module init
ModuleManager::register_module(module);
module->register_rpc(global_rpc_registrar);

// In registrar
std::map<std::string, JsonRpcHandler> methods_;
methods_["module.method"] = handler_function;
```

#### Execution Phase
```cpp
// API request arrives
esp_err_t execute(const std::string& method, 
                 const json& params,
                 json& result) {
    auto it = methods_.find(method);
    if (it != methods_.end()) {
        return it->second(params, result);
    }
    return ESP_ERR_NOT_FOUND;
}
```

### Optimization Techniques

#### 1. String Deduplication
```python
# Generator identifies duplicate strings
all_strings = set()
for schema in schemas:
    collect_strings(schema, all_strings)
    
# Create string table
STRING_TABLE = create_dedup_table(all_strings)
```

#### 2. Binary Protocol Optimization
```cpp
// For Modbus - direct binary mapping
struct ModbusRegister {
    uint16_t address;
    uint8_t type;  // HOLDING, INPUT, etc
    float scale;   // For fixed-point conversion
    const char* rpc_method;
};
```

#### 3. Conditional Compilation
```cpp
#ifdef CONFIG_UI_WEB_ENABLED
    #include "generated/web_ui_generated.h"
#endif

#ifdef CONFIG_UI_MQTT_ENABLED
    #include "generated/mqtt_topics_generated.h"
#endif
```

### Thread Safety

#### SharedState Access
```cpp
// All SharedState operations are thread-safe
template<typename T>
bool SharedState::get(const std::string& key, T& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    return get_internal(key, value);
}
```

#### EventBus Publishing
```cpp
// Events queued if called from non-main task
if (xTaskGetCurrentTaskHandle() != main_task_handle_) {
    xQueueSend(event_queue_, &event, portMAX_DELAY);
} else {
    process_event(event);
}
```

### Error Handling

#### RPC Errors
```cpp
// Standard error responses
{"jsonrpc": "2.0", "id": 1, "error": {
    "code": -32602,  // Invalid params
    "message": "Missing required parameter: value"
}}
```

#### UI Adapter Errors
```cpp
// Graceful degradation
if (mqtt_connect() != ESP_OK) {
    ESP_LOGW(TAG, "MQTT unavailable, disabling");
    disable_mqtt_features();
    // Other UIs continue working
}
```

### Performance Metrics

#### Typical Response Times
| Operation | Time | Notes |
|-----------|------|-------|
| Web page load | 50-100ms | From PROGMEM |
| API data request | 10-20ms | Cached values |
| RPC method call | 5-15ms | Direct execution |
| MQTT publish | 2-5ms | Async queue |

#### Memory Footprint
| Component | RAM | Flash |
|-----------|-----|-------|
| Web UI runtime | 3-5KB | 30-40KB |
| MQTT runtime | 2-3KB | 10-15KB |
| LCD runtime | 1-2KB | 5-10KB |
| RPC registry | 1-2KB | 2-3KB |

### Extension Points

#### Custom Control Types
```cpp
// In ui_generator.py
def render_custom_control(control):
    if control['type'] == 'custom_gauge':
        return generate_custom_gauge_html(control)
```

#### Protocol-specific Features
```json
{
    "controls": [...],
    "mqtt_ui": {
        "discovery_prefix": "homeassistant",
        "device_class": "temperature"
    },
    "modbus_ui": {
        "start_address": 40100,
        "byte_order": "big_endian"
    }
}
```

### Build System Integration

#### CMake Configuration
```cmake
# Automatic dependency tracking
file(GLOB_RECURSE UI_SCHEMAS ${COMPONENT_SRCS_DIRS}/*/ui_schema.json)
add_custom_command(
    OUTPUT ${GENERATED_HEADERS}
    DEPENDS ${UI_SCHEMAS} ${UI_GENERATOR_SCRIPT}
    COMMAND ${Python3_EXECUTABLE} ${UI_GENERATOR_SCRIPT}
)
```

#### Incremental Builds
- Only regenerates if schemas change
- Caches parsed schemas
- Parallel generation possible

### Debugging

#### Enable Debug Output
```cpp
// In sdkconfig
CONFIG_UI_DEBUG_LEVEL=4

// In code
ESP_LOGD(TAG, "Processing element: %s", elem.id);
```

#### Schema Validation
```bash
# Validate all schemas
python tools/ui_generator.py --validate-only

# Check specific schema
python tools/validate_schema.py components/module/ui_schema.json
```

#### Runtime Inspection
```cpp
// Get all registered RPC methods
GET /api/rpc/methods

// Get UI schema for module
GET /api/schema/module_name

// Test RPC method
POST /api/rpc/test
{
    "method": "module.get_value",
    "params": {}
}
```

### Security Considerations

#### Input Validation
```cpp
// All RPC inputs validated
if (!params.is_object()) {
    return ESP_ERR_INVALID_ARG;
}

// Range checking
if (value < MIN_ALLOWED || value > MAX_ALLOWED) {
    return ESP_ERR_INVALID_ARG;
}
```

#### Access Control
```cpp
// Optional authentication
if (config_.auth_required && !authenticate(req)) {
    httpd_resp_set_status(req, "401 Unauthorized");
    return ESP_FAIL;
}
```

---

*For implementation examples and practical usage, see the Quick Start Guide and main documentation.*