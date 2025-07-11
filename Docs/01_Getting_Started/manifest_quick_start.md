# Phase 5: Quick Start Guide for Developers

## 🚀 TL;DR - What's Phase 5?

**Revolutionary UI architecture** that generates ALL possible UI components at build-time, filters them at runtime based on configuration/role, and lazy-loads only what's needed.

**Benefits**: 0ms runtime generation, 20-40% RAM usage, type-safe, deterministic behavior.

## 🎯 For Module Developers

### 1. Update Your Module Manifest

#### Застарілий підхід:
```json
{
  "module": {
    "name": "MyModule"
  },
  "ui": {
    "pages": [
      {
        "id": "my_page",
        "widgets": [...]
      }
    ]
  }
}
```

#### Сучасний підхід (Adaptive UI):
```json
{
  "module": {
    "name": "MyModule"
  },
  "ui_components": {
    "all_possible": [
      {
        "id": "my_widget_basic",
        "type": "text",
        "applicability": ["always"],
        "access_level": "user",
        "priority": "high"
      },
      {
        "id": "my_widget_advanced", 
        "type": "slider",
        "applicability": ["config.mode == 'advanced'"],
        "access_level": "technician",
        "priority": "medium"
      }
    ]
  }
}
```

### 2. No Code Changes Required!

Your existing module code works as-is. The UI system handles everything:

```cpp
// Your module stays the same
class MyModule : public BaseModule {
    // No changes needed!
};
```

## 🎯 For Driver Developers

### 1. Create Driver Manifest with UI Extensions

```json
{
  "driver": {
    "name": "MyDriver",
    "type": "sensor"
  },
  "ui_extensions": [
    {
      "id": "my_driver_config",
      "condition": "config.sensor.type == 'MyDriver'",
      "inject_into": "sensor_config_panel",
      "component": {
        "type": "number_input",
        "label": "Sampling Rate",
        "access_level": "technician"
      }
    }
  ]
}
```

### 2. UI Automatically Appears When Driver Selected!

No code needed - the system handles injection based on conditions.

## 🔧 How to Enable Phase 5

### 1. Build Configuration

```cmake
# In your CMakeLists.txt or sdkconfig
set(USE_ADAPTIVE_UI ON)
set(UI_LAZY_LOADING ON)
set(UI_MEMORY_LIMIT 50)  # KB
```

### 2. Rebuild with New Generator

```bash
# Clean build to regenerate everything
idf.py fullclean
idf.py build
```

### 3. Runtime Configuration

```json
// In system_config.json
{
  "ui": {
    "adaptive": true,
    "lazy_loading": true,
    "priority_components": [
      "main_menu",
      "status_display",
      "error_dialog"
    ]
  }
}
```

## 📊 Performance Comparison

| Metric | Old System | Phase 5 |
|--------|------------|---------|
| UI Generation Time | 50-200ms | 0ms |
| RAM Usage (100 components) | 25KB | 5-10KB |
| Startup Time | 2-3s | <1s |
| Adding New Component | Rebuild | Manifest only |

## 🎮 Real Examples

### Example 1: Dynamic Sensor UI

When user selects "DS18B20" sensor:
```
Before selection:
└── Sensors
    └── Sensor Type: [Dropdown]

After selection:
└── Sensors
    ├── Sensor Type: [DS18B20]
    ├── Resolution: [====|==] 12-bit  ← Appears!
    ├── Parasitic Power: [ON/OFF]     ← Appears!
    └── Bus Address: 28:FF:64:1E:...  ← Appears!
```

### Example 2: Role-Based UI

```
User login:
└── Sensors
    └── Temperature: 23.5°C

Technician login:
└── Sensors  
    ├── Temperature: 23.5°C
    ├── Calibrate [Button]     ← Appears for technician!
    └── Offset: +0.5°C         ← Appears for technician!
```

## 🐛 Troubleshooting

### UI Components Not Showing?

1. Check manifest syntax:
```bash
python tools/validate_manifest.py components/mymodule/module_manifest.json
```

2. Verify condition evaluation:
```cpp
ESP_LOGI(TAG, "Condition result: %d", evaluator.evaluate(config));
```

3. Check access level:
```cpp
ESP_LOGI(TAG, "User role: %d, Required: %d", current_role, component->access_level);
```

### High Memory Usage?

1. Reduce priority components
2. Lower memory limit
3. Simplify conditions

### Performance Issues?

1. Enable condition caching
2. Reduce filter frequency
3. Profile with ESP-IDF tools

## 🎯 Best Practices

### DO:
- ✅ Use simple, readable conditions
- ✅ Set appropriate priorities
- ✅ Test with different roles
- ✅ Monitor memory usage

### DON'T:
- ❌ Create complex nested conditions
- ❌ Mark everything as priority
- ❌ Assume components are always loaded
- ❌ Bypass the lazy loading system

## 📚 More Resources

- [Technical Specification](PHASE5_TECHNICAL_SPEC.md)
- [Architecture Overview](ADAPTIVE_UI_ARCHITECTURE.md)
- [Implementation Plan](PHASE5_IMPLEMENTATION_PLAN.md)
- [Migration Guide](PHASE5_MIGRATION_GUIDE.md)

## 💡 Quick Commands

```bash
# Validate all manifests
python tools/validate_all_manifests.py

# Generate UI report
python tools/ui_report.py --show-conditions

# Test adaptive UI
idf.py test --filter "adaptive_ui"

# Benchmark performance
python tools/benchmark_ui.py --compare-legacy
```

## 🆘 Need Help?

1. Check existing examples in `components/*/module_manifest.json`
2. Run validation tools
3. Enable debug logging: `#define ADAPTIVE_UI_DEBUG 1`
4. Ask in project chat with `@phase5` tag

**Remember**: Phase 5 is backward compatible - your existing modules continue to work!