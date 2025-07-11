# ModESP Manifest System - Quick Reference

## Creating a New Module

### 1. Create Module Structure
```bash
components/
└── my_module/
    ├── CMakeLists.txt
    ├── my_module.h
    ├── my_module.cpp
    └── module_manifest.json
```

### 2. Write Module Manifest
```json
{
  "name": "MyModule",
  "type": "standard",
  "version": "1.0.0",
  "description": "My awesome module",
  "priority": "normal",
  "dependencies": ["SharedState", "EventBus"],
  "config_file": "mymodule.json",
  "api": [
    {
      "method": "mymodule.get_status",
      "description": "Get module status",
      "params": {},
      "returns": {"type": "object"},
      "access_level": "user"
    }
  ],
  "events": [
    {
      "name": "mymodule.state_changed",
      "description": "Module state changed",
      "payload": {
        "state": "New state value",
        "previous": "Previous state value"
      }
    }
  ]
}
```

### 3. Implement Module
```cpp
#include "my_module.h"
#include "module_factory.h"
#include "event_helpers.h"

class MyModule : public BaseModule {
public:
    const char* get_name() const override { return "MyModule"; }
    
    esp_err_t init() override {
        // Initialize module
        return ESP_OK;
    }
    
    void update() override {
        // Module logic
    }
    
    // ... other methods ...
};

// Register with factory
REGISTER_MODULE(MyModule);
```

### 4. Process Manifests
```bash
python tools/process_manifests.py \
    --project-root . \
    --output-dir main/generated
```

## Using Type-Safe Events

### Publishing Events
```cpp
// Method 1: Type-safe publisher
EventPublisher::publishSensorError("temp_sensor", "Timeout", -1);

// Method 2: Using generated constants  
EventBus::publish(Events::SENSOR_ERROR, data);

// Method 3: Macro (compile-time checked)
PUBLISH_EVENT(SENSOR_ERROR, data);
```

### Subscribing to Events
```cpp
// Method 1: Type-safe subscriber
auto handle = EventSubscriber::onSensorError(
    [](const std::string& sensor, const std::string& error, int code) {
        // Handle error
    });

// Method 2: Direct subscription
EventBus::subscribe(Events::SENSOR_ERROR, [](const Event& e) {
    // Handle event
});

// Method 3: Macro
SUBSCRIBE_EVENT(SENSOR_ERROR, handler);
```

## Module Registration Flow

### Automatic Registration
```cpp
// In main.cpp or system init
ModuleManager::init();
ModuleManager::register_modules_from_manifests();
```

### Manual Registration
```cpp
auto module = std::make_unique<MyModule>();
ModuleManager::register_module(std::move(module), ModuleType::STANDARD);
```

## Generated Files Reference

### `generated_module_info.h/cpp`
- Module metadata from manifests
- Dependencies and configuration

### `generated_api_registry.h/cpp`
- All API methods from manifests
- Auto-registration function

### `generated_events.h`
- Event name constants
- Compile-time checking

### `generated_ui_schemas.h`
- UI configuration data
- Widget definitions

## Common Patterns

### Module with Events
```cpp
void MyModule::update() {
    if (state_changed) {
        nlohmann::json data = {
            {"state", new_state},
            {"previous", old_state}
        };
        EventBus::publish("mymodule.state_changed", data);
    }
}
```

### Module with RPC
```cpp
void MyModule::register_rpc(IJsonRpcRegistrar& reg) {
    reg.register_method("mymodule.get_status",
        [this](const json& params, json& result) {
            result["status"] = get_status();
            return ESP_OK;
        });
}
```

### Dependency Injection
```cpp
// For modules needing system resources
class SensorModule : public BaseModule {
    ESPhal& hal_;
public:
    SensorModule(ESPhal& hal) : hal_(hal) {}
};

// Factory function
static std::unique_ptr<BaseModule> createSensorModule() {
    extern ESPhal* g_hal;  // Set during system init
    return std::make_unique<SensorModule>(*g_hal);
}
```

## Debugging Tips

### Check Manifest Loading
```cpp
auto& reader = ManifestReader::getInstance();
reader.dumpManifestInfo("MyModule");
```

### Verify Module Registration
```cpp
ModuleManager::dump_modules("SystemInit");
```

### Event Validation
```cpp
auto& validator = EventValidator::getInstance();
if (!validator.isValidEvent("my.event")) {
    ESP_LOGE(TAG, "Invalid event!");
}
```

### List All Events
```cpp
auto events = validator.getAllEvents();
for (const auto& event : events) {
    ESP_LOGI(TAG, "Event: %s", event.c_str());
}
```

## Best Practices

1. **Always validate JSON** in configure()
2. **Use generated constants** for events
3. **Document APIs** in manifest
4. **Keep manifests in sync** with code
5. **Run manifest processor** before building
6. **Test with example module** first
7. **Use type-safe helpers** when possible
8. **Handle errors gracefully**

## Troubleshooting

### Module Not Found
- Check REGISTER_MODULE() macro
- Verify manifest name matches
- Run manifest processor
- Check CMakeLists.txt

### Event Not Valid
- Add to module manifest
- Run manifest processor
- Check event name format
- Enable dynamic patterns

### Dependencies Missing
- List in manifest
- Ensure load order
- Check circular deps
- Verify module names

### RPC Not Working
- Check has_rpc() returns true
- Verify method name
- Check access level
- Test with curl/postman
