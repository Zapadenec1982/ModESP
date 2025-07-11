# Example Module Template

This directory contains a complete example of a module that integrates with the ModESP manifest-driven architecture.

## Files

- **example_module.h/cpp** - Module implementation
- **module_manifest.json** - Module manifest declaring metadata, dependencies, APIs, and UI
- **example.json** - Example configuration file

## Key Features Demonstrated

### 1. Module Registration
The module registers itself with the factory using:
```cpp
REGISTER_MODULE(ExampleModule);
```

### 2. Manifest Integration
The manifest declares:
- Module metadata (name, version, type)
- Dependencies on SharedState and EventBus
- API methods with access levels
- Events published by the module
- UI schema for configuration

### 3. Health Monitoring
The module implements:
- `is_healthy()` - Basic health check
- `get_health_score()` - Detailed health score (0-100)

### 4. State Management
- Publishes state to SharedState
- Subscribes to system events
- Publishes custom events

### 5. RPC Support
Registers two RPC methods:
- `example.get_status` - Get module status
- `example.reset_counter` - Reset internal counter

## How to Use This Template

1. **Copy the files** to your module directory
2. **Rename** files and update class names
3. **Update manifest** with your module's information
4. **Implement** your module-specific logic in `do_work()`
5. **Add to build** system (CMakeLists.txt)
6. **Run manifest processor** to generate code

## Integration Steps

### Step 1: Create Your Module
```bash
cp -r docs/module_manifest_architecture/examples components/my_module
cd components/my_module
# Rename files and update content
```

### Step 2: Update CMakeLists.txt
```cmake
idf_component_register(
    SRCS "my_module.cpp"
    INCLUDE_DIRS "."
    REQUIRES base_module core
)
```

### Step 3: Process Manifests
```bash
python tools/process_manifests.py \
    --project-root . \
    --output-dir main/generated
```

### Step 4: Build and Test
```bash
idf.py build
idf.py flash monitor
```

## Best Practices

1. **Always validate JSON** in configure()
2. **Implement proper health checks**
3. **Use EventBus for loose coupling**
4. **Document your APIs in the manifest**
5. **Keep update() fast** (< 2ms typical)
6. **Handle errors gracefully**

## Common Patterns

### Periodic Tasks
```cpp
void update() {
    uint32_t now = esp_timer_get_time() / 1000;
    if (now - last_run_ >= interval_ms_) {
        do_periodic_task();
        last_run_ = now;
    }
}
```

### Event Handling
```cpp
EventBus::subscribe("some.event", [this](const json& data) {
    handle_event(data);
});
```

### State Publishing
```cpp
SharedState::set("module.key", {
    {"value", 42},
    {"timestamp", esp_timer_get_time()}
});
```

### Safe Configuration
```cpp
if (config.contains("key") && config["key"].is_number()) {
    value_ = config["key"];
}
```

## Troubleshooting

### Module Not Loading
- Check manifest syntax
- Verify REGISTER_MODULE matches manifest name
- Run manifest processor
- Check dependencies are available

### RPC Not Working
- Ensure has_rpc() returns true
- Check method names match manifest
- Verify JSON-RPC registration

### Health Issues
- Implement meaningful health checks
- Update health score based on real conditions
- Log health problems for debugging
