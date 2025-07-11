# 🚀 Full Migration to Phase 5 - Action Plan

## Overview
Since Phase 2 doesn't align with the project vision and Phase 5 is the true architecture, we'll do a complete migration.

## 🧹 Step 1: Clean Up Phase 2 Artifacts

### 1.1 Remove Old Generated Files
```bash
cd C:\ModESP_dev

# Remove old UI generation
rm main/generated/generated_ui_schemas.h
rm main/generated/lcd_menu_generated.h  
rm main/generated/web_ui_generated.h
rm main/generated/ui_registry_generated.h

# Keep these (still needed):
# - generated_api_registry.cpp/h
# - generated_events.h
# - generated_module_info.cpp/h
```

### 1.2 Update process_manifests.py
Remove old UI generation code and keep only:
- API generation
- Event generation
- Module info generation
- NEW: Adaptive UI generation

### 1.3 Clean Module Manifests
Remove from all `module_manifest.json`:
- `"ui_schema"` sections
- `"ui"` sections (except new `"adaptive"`)

Add to Manager modules:
- `"type": "MANAGER"`
- `"driver_interface": "IDriverInterface"`
- `"ui": { "adaptive": { ... } }`

## 🏗️ Step 2: Restructure for Phase 5

### 2.1 Create New Structure
```bash
# Create adaptive UI directory
mkdir -p components/adaptive_ui
mkdir -p components/adaptive_ui/include

# Move Phase 5 components
mv components/core/ui_component_base.h components/adaptive_ui/include/
mv components/core/ui_filter.* components/adaptive_ui/
mv components/core/lazy_component_loader.* components/adaptive_ui/
mv components/core/module_manager_adaptive.h components/adaptive_ui/include/
mv components/core/base_driver.h components/adaptive_ui/include/
```

### 2.2 Create CMakeLists for Adaptive UI
```cmake
# components/adaptive_ui/CMakeLists.txt
idf_component_register(
    SRCS 
        "ui_filter.cpp"
        "lazy_component_loader.cpp"
        # Add renderers when implemented
        "lcd_renderer.cpp"
        "web_renderer.cpp"
        "mqtt_renderer.cpp"
    INCLUDE_DIRS "include"
    REQUIRES 
        base_module
        mittelab__nlohmann-json
        esp_timer
    PRIV_REQUIRES
        log
)
```

### 2.3 Update Core CMakeLists
Remove from `components/core/CMakeLists.txt`:
- `"ui_filter.cpp"`
- `"lazy_component_loader.cpp"`

Add to REQUIRES:
- `adaptive_ui`

## 📝 Step 3: Update All Manifests

### 3.1 SensorModule → SensorManager
```json
{
  "module": {
    "name": "SensorManager",
    "type": "MANAGER",
    "driver_interface": "ISensorDriver",
    "version": "2.0.0",
    "description": "Adaptive sensor management with dynamic drivers"
  },
  
  "ui": {
    "adaptive": {
      "components": [
        {
          "id": "sensor_overview",
          "type": "composite",
          "conditions": ["always"],
          "access_level": "user",
          "priority": "high",
          "sub_components": []  // Filled by drivers
        },
        {
          "id": "sensor_config",
          "type": "composite", 
          "conditions": ["role >= 'technician'"],
          "access_level": "technician",
          "priority": "medium",
          "sub_components": []  // Filled by drivers
        }
      ]
    }
  },
  
  "driver_registry": {
    "path": "sensor_drivers",
    "pattern": "*_driver_manifest.json",
    "interface": "ISensorDriver"
  }
}
```

### 3.2 ActuatorModule → ActuatorManager
Similar pattern - convert to Manager with driver support

### 3.3 Update All Driver Manifests
Add `ui_extensions` to each driver manifest

## 🔧 Step 4: Update Module Implementations

### 4.1 Convert Modules to Managers
```cpp
// Before: SensorModule
class SensorModule : public BaseModule {
    // Monolithic implementation
};

// After: SensorManager  
class SensorManager : public BaseModule {
private:
    std::vector<std::unique_ptr<ISensorDriver>> drivers;
    
public:
    esp_err_t registerDriver(std::unique_ptr<ISensorDriver> driver);
    std::vector<std::string> getAllUIComponents() const;
};
```

### 4.2 Implement Base Drivers
- Create `ISensorDriver` implementations
- Create `IActuatorDriver` implementations
- Each driver provides its UI components

## 🎨 Step 5: Implement Renderers

### 5.1 LCD Renderer
```cpp
class LCDRenderer {
    void renderMenu(const std::vector<UIComponent*>& components);
    void renderValue(const TextComponent& text);
    void renderSlider(const SliderComponent& slider);
};
```

### 5.2 Web Renderer
```cpp
class WebRenderer {
    std::string renderHTML(const std::vector<UIComponent*>& components);
    std::string renderJSON(const std::vector<UIComponent*>& components);
};
```

## 📋 Step 6: Update Documentation

### 6.1 Archive Phase 2 Docs
```bash
mkdir -p Docs/_archive/phase2_complete
mv Docs/module_manifest_architecture/PHASE2_*.md Docs/_archive/phase2_complete/
```

### 6.2 Update Main Docs
- Update `README.md` - Phase 5 as main architecture
- Update `ARCHITECTURE_OVERVIEW.md` - Adaptive UI as core
- Create `MIGRATION_COMPLETE.md` - Document the change

## ✅ Step 7: Validation

### 7.1 Test Build
```bash
# Clean build
idf.py fullclean
idf.py build
```

### 7.2 Run Tests
```bash
# Run adaptive UI test
idf.py flash monitor
# Check for: "Phase 5 Adaptive UI Test"
```

### 7.3 Verify Components
- All managers have drivers
- All drivers provide UI components  
- Filter works correctly
- Lazy loading functions

## 🎯 Success Criteria

- [ ] No Phase 2 UI code remains
- [ ] All modules converted to Manager pattern
- [ ] Drivers implemented for all hardware
- [ ] UI components generate correctly
- [ ] System builds without errors
- [ ] Tests pass

## 🚀 Let's Start!

Ready to make Phase 5 the ONE TRUE architecture?

Start with Step 1.1 - Clean up old files!
