# Phase 5: Migration Guide

## 📋 Overview

This guide helps you migrate existing ModESP modules from Phase 2 (current system) to Phase 5 (Adaptive UI Architecture).

## 🎯 Migration Strategy

### Recommended Approach: Gradual Migration

1. **Phase 5.1**: Enable Phase 5 in parallel mode
2. **Phase 5.2**: Migrate one module at a time
3. **Phase 5.3**: Test and compare performance
4. **Phase 5.4**: Switch to Phase 5 only mode
5. **Phase 5.5**: Remove legacy code

## 🔄 Step-by-Step Migration

### Step 1: Enable Parallel Mode

```cmake
# CMakeLists.txt
option(USE_ADAPTIVE_UI "Enable Phase 5 Adaptive UI" ON)
option(KEEP_LEGACY_UI "Keep Phase 2 UI for comparison" ON)
```

### Step 2: Update Module Manifest

#### Example: SensorModule Migration

**Phase 2 Manifest (OLD):**
```json
{
  "module": {
    "name": "SensorModule"
  },
  "ui": {
    "static": {
      "pages": [
        {
          "id": "sensor_overview",
          "widgets": [
            {
              "type": "text",
              "id": "temp_display",
              "value_source": "state.sensor.temperature"
            }
          ]
        }
      ]
    }
  }
}
```

**Phase 5 Manifest (NEW):**
```json
{
  "module": {
    "name": "SensorModule"
  },
  "ui": {
    "static": {
      // Keep for backward compatibility
    }
  },
  "ui_components": {
    "all_possible": [
      {
        "id": "temp_display",
        "type": "text",
        "label": "Temperature",
        "value_source": "state.sensor.temperature",
        "applicability": ["always"],
        "access_level": "user",
        "priority": "high",
        "channels": ["lcd", "web", "mqtt"]
      },
      {
        "id": "temp_calibrate_btn",
        "type": "button",
        "label": "Calibrate",
        "action": "sensor.calibrate",
        "applicability": ["has_capability('calibration')"],
        "access_level": "technician",
        "priority": "low",
        "channels": ["lcd", "web"]
      }
    ]
  }
}
```

### Step 3: Update Driver Manifests

**Add UI Extensions to Drivers:**
```json
{
  "driver": {
    "name": "DS18B20AsyncDriver"
  },
  "ui_extensions": [
    {
      "id": "ds18b20_resolution_slider",
      "condition": "config.sensor.type == 'DS18B20'",
      "inject_into": "sensor_config",
      "component": {
        "type": "slider",
        "label": "Resolution (bits)",
        "min": 9,
        "max": 12,
        "value_source": "config.ds18b20.resolution",
        "access_level": "technician"
      }
    }
  ]
}
```

### Step 4: Test in Parallel Mode

```cpp
// In your main.cpp
void app_main() {
    // Initialize both systems
    ModuleManager::init();
    
    #ifdef USE_ADAPTIVE_UI
    AdaptiveUI::init();
    #endif
    
    // Compare performance
    if (Config::isDebugMode()) {
        BenchmarkUI::comparePhase2vsPhase5();
    }
}
```

### Step 5: Validate Migration

Run validation script:
```bash
python tools/validate_migration.py --module SensorModule
```

Expected output:
```
✓ Manifest valid for Phase 5
✓ All UI elements migrated
✓ Conditions properly formatted
✓ Access levels defined
✓ Priority assignments correct
⚠ Legacy UI still present (OK for parallel mode)
```

## 📊 Manifest Field Mapping

| Phase 2 Field | Phase 5 Field | Notes |
|---------------|---------------|-------|
| `ui.pages[].widgets[]` | `ui_components.all_possible[]` | Flat structure in Phase 5 |
| `widget.type` | `component.type` | Same values |
| `widget.id` | `component.id` | Must be unique |
| N/A | `component.applicability` | New: conditions array |
| N/A | `component.access_level` | New: role-based access |
| N/A | `component.priority` | New: for lazy loading |
| N/A | `component.channels` | New: multi-channel support |

## 🔧 Code Updates (If Needed)

### Most modules need NO code changes!

However, if you have custom UI logic:

**Phase 2 (OLD):**
```cpp
void MyModule::buildUI() {
    if (hasFeature("advanced")) {
        ui->addWidget(new SliderWidget(...));
    }
}
```

**Phase 5 (NEW):**
```cpp
// Remove buildUI() - everything is declarative in manifest!
// The condition moves to manifest:
// "applicability": ["has_feature('advanced')"]
```

## 🎮 Testing Your Migration

### 1. Functional Test
```cpp
TEST_CASE("Module UI works in Phase 5") {
    // Load module with Phase 5
    auto module = ModuleFactory::create("MyModule");
    
    // Get UI components
    auto components = AdaptiveUI::getComponents("MyModule");
    
    // Verify all components present
    REQUIRE(components.size() > 0);
}
```

### 2. Performance Test
```bash
# Run benchmark
python tools/benchmark_ui.py --module MyModule

# Expected output:
# Phase 2: RAM: 15KB, Load time: 150ms
# Phase 5: RAM: 6KB, Load time: 20ms
# Improvement: 60% less RAM, 87% faster
```

### 3. User Experience Test
- Login with different roles
- Change configuration
- Verify UI morphs correctly

## ⚠️ Common Migration Issues

### Issue 1: Components Not Showing

**Problem**: UI components don't appear after migration

**Solution**: Check applicability conditions
```json
// Wrong
"applicability": ["config.sensor_type == 'DS18B20'"]

// Correct  
"applicability": ["config.sensor.type == 'DS18B20'"]
```

### Issue 2: Performance Degradation

**Problem**: Phase 5 slower than Phase 2

**Solution**: Check priority settings
```json
// Set high priority for frequently used components
"priority": "high"  // Preloaded at startup
"priority": "medium" // Loaded on first use
"priority": "low"   // Loaded on demand
```

### Issue 3: Memory Warnings

**Problem**: "Component memory limit exceeded"

**Solution**: Adjust memory limit or reduce components
```json
{
  "ui": {
    "memory_limit_kb": 75  // Increase from default 50KB
  }
}
```

## 📈 Migration Checklist

- [ ] Enable Phase 5 in build configuration
- [ ] Update module manifest with `ui_components`
- [ ] Add UI extensions to driver manifests
- [ ] Validate manifests with tools
- [ ] Test in parallel mode
- [ ] Compare performance metrics
- [ ] Test all user roles
- [ ] Test configuration changes
- [ ] Document any custom changes
- [ ] Remove legacy UI (when ready)

## 🚀 Advanced Migration

### Custom Component Types

If you have custom UI components:

1. Register component type:
```cpp
ComponentRegistry::registerType("my_custom_widget", 
    []() { return new MyCustomWidget(); }
);
```

2. Use in manifest:
```json
{
  "type": "my_custom_widget",
  "config": {
    "custom_param": "value"
  }
}
```

### Complex Conditions

For complex logic, use condition expressions:
```json
"applicability": [
  "(config.mode == 'advanced' AND has_feature('pro')) OR user_role >= 'admin'"
]
```

## 📚 Resources

- [Phase 5 Technical Spec](PHASE5_TECHNICAL_SPEC.md)
- [Manifest Examples](examples/)
- [Validation Tools](../../tools/README.md)
- [Performance Benchmarks](PHASE5_BENCHMARKS.md)

## 🆘 Getting Help

1. Run validation first: `python tools/validate_manifest.py`
2. Check examples in `components/*/module_manifest.json`
3. Enable debug logging: `CONFIG_ADAPTIVE_UI_DEBUG=y`
4. Ask specific questions with error logs

**Remember**: Migration is gradual - your existing code continues to work!