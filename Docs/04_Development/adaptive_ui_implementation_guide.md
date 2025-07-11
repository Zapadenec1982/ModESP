# Phase 5 Implementation Guide: Adaptive UI Architecture

## 📋 Executive Summary

Phase 5 впроваджує революційну тришарову архітектуру UI генерації:
- **Build-time**: Генерація ВСІХ можливих компонентів (0ms runtime overhead)
- **Runtime**: Smart фільтрація за конфігурацією/роллю (O(n) complexity)
- **Lazy Loading**: Завантаження тільки потрібних компонентів (20-40% RAM usage)

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────┐
│         Build-time Generation               │
│  Manager Manifests + Driver Manifests       │
│                  ↓                          │
│         Component Generator                 │
│                  ↓                          │
│      ALL Possible UI Components             │
└─────────────────────┬───────────────────────┘
                      │
┌─────────────────────┴───────────────────────┐
│           Runtime Filtering                 │
│   UIFilter + Condition Evaluator            │
│                  ↓                          │
│       Visible Components List               │
└─────────────────────┬───────────────────────┘
                      │
┌─────────────────────┴───────────────────────┐
│           Lazy Loading Layer                │
│    LazyComponentLoader + Cache              │
│                  ↓                          │
│        Rendered UI Components               │
└─────────────────────────────────────────────┘
```

## 🎯 Implementation Steps

### Step 1: Extend Manifest Structure

#### 1.1 Update Manager Manifests
```json
{
  "module": {
    "name": "SensorModule",
    "type": "MANAGER",  // New type for managers
    "driver_interface": "ISensorDriver"
  },
  
  "ui": {
    "adaptive": {
      "components": [
        {
          "id": "sensor_type_selector",
          "type": "dropdown",
          "conditions": ["always"],
          "access_level": "user",
          "priority": "high",
          "lazy_load": false  // Always loaded
        },
        {
          "id": "resolution_control",
          "type": "composite",
          "conditions": ["config.sensor.type != 'none'"],
          "access_level": "technician",
          "priority": "medium",
          "lazy_load": true,
          "sub_components": []  // Filled by drivers
        }
      ]
    }
  }
}
```

#### 1.2 Update Driver Manifests
```json
{
  "driver": {
    "name": "DS18B20Driver",
    "implements": "ISensorDriver"
  },
  
  "ui_extensions": {
    "inject_into": "resolution_control",
    "components": [
      {
        "id": "ds18b20_resolution",
        "type": "slider",
        "condition": "config.sensor.type == 'DS18B20'",
        "config": {
          "min": 9,
          "max": 12,
          "label": "Resolution (bits)"
        }
      }
    ]
  }
}
```

### Step 2: Build-time Component Generator

#### 2.1 Extend process_manifests.py
```python
class AdaptiveUIGenerator:
    def generate_all_components(self):
        """Generate all possible UI components from manifests"""
        components = []
        
        # Process manager components
        for module in self.modules:
            if module.type == "MANAGER":
                components.extend(self.process_manager_ui(module))
        
        # Process driver extensions
        for driver in self.drivers:
            components.extend(self.process_driver_ui(driver))
        
        # Generate C++ code
        self.generate_component_registry(components)
        self.generate_component_factories(components)
        self.generate_filter_metadata(components)
```

#### 2.2 Generated Component Structure
```cpp
// generated_ui_components.h
namespace ModESP::UI {

struct ComponentMetadata {
    const char* id;
    ComponentType type;
    const char* condition;
    AccessLevel min_access;
    Priority priority;
    bool lazy_loadable;
    size_t estimated_size;
};

// All possible components as compile-time constants
constexpr ComponentMetadata ALL_COMPONENTS[] = {
    {"sensor_type_selector", ComponentType::DROPDOWN, "always", 
     AccessLevel::USER, Priority::HIGH, false, 256},
    {"ds18b20_resolution", ComponentType::SLIDER, 
     "config.sensor.type == 'DS18B20'", 
     AccessLevel::TECHNICIAN, Priority::MEDIUM, true, 128},
    // ... all other components
};

constexpr size_t COMPONENT_COUNT = sizeof(ALL_COMPONENTS) / sizeof(ComponentMetadata);

} // namespace ModESP::UI
```

### Step 3: Smart Filter Implementation

#### 3.1 Condition Evaluator
```cpp
// ui_filter.h
class ConditionEvaluator {
private:
    const nlohmann::json& config;
    UserRole current_role;
    
public:
    bool evaluate(const std::string& condition) {
        if (condition == "always") return true;
        
        // Parse and evaluate conditions
        // Examples:
        // - "config.sensor.type == 'DS18B20'"
        // - "has_feature('calibration')"
        // - "role >= 'technician'"
        
        return evaluateExpression(condition);
    }
};
```

#### 3.2 UI Filter Engine
```cpp
// ui_filter.cpp
class UIFilter {
private:
    ConditionEvaluator evaluator;
    
public:
    std::vector<ComponentMetadata> filterComponents(
        const ComponentMetadata* all_components,
        size_t count,
        const nlohmann::json& config,
        UserRole role
    ) {
        std::vector<ComponentMetadata> visible;
        
        for (size_t i = 0; i < count; ++i) {
            const auto& comp = all_components[i];
            
            // Check condition
            if (!evaluator.evaluate(comp.condition)) continue;
            
            // Check access level
            if (!hasAccess(role, comp.min_access)) continue;
            
            visible.push_back(comp);
        }
        
        return visible;
    }
};
```

### Step 4: Lazy Loading System

#### 4.1 Component Factory Registry
```cpp
// component_factory.h
class ComponentFactory {
private:
    using FactoryFunc = std::function<std::unique_ptr<UIComponent>()>;
    std::unordered_map<std::string, FactoryFunc> factories;
    
public:
    void registerFactory(const std::string& id, FactoryFunc factory) {
        factories[id] = factory;
    }
    
    std::unique_ptr<UIComponent> create(const std::string& id) {
        auto it = factories.find(id);
        if (it != factories.end()) {
            return it->second();
        }
        return nullptr;
    }
};
```

#### 4.2 Lazy Component Loader
```cpp
// lazy_loader.h
class LazyComponentLoader {
private:
    ComponentFactory factory;
    std::unordered_map<std::string, std::unique_ptr<UIComponent>> cache;
    std::set<std::string> priority_components;
    
    // Memory management
    size_t max_cache_size = 10 * 1024; // 10KB
    size_t current_cache_size = 0;
    
public:
    UIComponent* getComponent(const std::string& id) {
        // Check cache first
        auto it = cache.find(id);
        if (it != cache.end()) {
            return it->second.get();
        }
        
        // Create component
        auto component = factory.create(id);
        if (!component) return nullptr;
        
        // Check memory limits
        size_t comp_size = component->getEstimatedSize();
        if (current_cache_size + comp_size > max_cache_size) {
            evictLRU();
        }
        
        // Cache and return
        current_cache_size += comp_size;
        UIComponent* ptr = component.get();
        cache[id] = std::move(component);
        
        return ptr;
    }
    
    void preloadPriority() {
        for (const auto& id : priority_components) {
            getComponent(id);
        }
    }
};
```

### Step 5: ModuleManager Integration

#### 5.1 Manager-Driver Composition Support
```cpp
// module_manager.cpp modifications
esp_err_t ModuleManager::register_manager_with_drivers(
    std::unique_ptr<BaseModule> manager,
    const std::string& driver_interface
) {
    // Register manager
    register_module(std::move(manager), ModuleType::HIGH);
    
    // Discover and register drivers
    auto& manifestReader = ManifestReader::getInstance();
    auto drivers = manifestReader.getDriversForInterface(driver_interface);
    
    for (const auto& driver_manifest : drivers) {
        // Create driver instance
        auto driver = DriverFactory::create(driver_manifest);
        if (driver) {
            // Register as sub-module
            register_driver(manager->get_name(), std::move(driver));
        }
    }
    
    return ESP_OK;
}
```

### Step 6: Multi-channel Adapters

#### 6.1 Unified Component Interface
```cpp
// ui_component.h
class UIComponent {
public:
    virtual void renderLCD(LCDRenderer& r) = 0;
    virtual void renderWeb(WebRenderer& r) = 0;
    virtual void renderMQTT(MQTTRenderer& r) = 0;
    virtual size_t getEstimatedSize() const = 0;
};

// Example implementation
class DS18B20ResolutionSlider : public UIComponent {
    void renderLCD(LCDRenderer& r) override {
        r.addSlider("Resolution", value, 9, 12);
    }
    
    void renderWeb(WebRenderer& r) override {
        r.addHTML(R"(
            <input type="range" 
                   id="ds18b20_res" 
                   min="9" max="12" 
                   value="%d">
        )", value);
    }
    
    void renderMQTT(MQTTRenderer& r) override {
        r.publishConfig("sensor/ds18b20/resolution", {
            {"type", "number"},
            {"min", 9},
            {"max", 12}
        });
    }
};
```

## 📊 Performance Metrics

### Memory Usage Comparison
| Approach | Build-time | Runtime | Peak RAM |
|----------|------------|---------|----------|
| Current (Phase 2) | 100KB | 8KB | 8KB |
| 80/20 Approach | 150KB | 12KB | 12KB |
| Adaptive (Phase 5) | 200KB | 3-5KB | 5-8KB |

### Timing Analysis
| Operation | Current | Adaptive |
|-----------|---------|----------|
| UI Init | 50ms | 10ms |
| Config Change | 100ms | 5ms |
| Component Access | 0.1ms | 0.5ms* |

*First access only, then cached

## 🧪 Testing Strategy

### 1. Unit Tests
```cpp
TEST_CASE("UIFilter filters components correctly") {
    // Test configuration-based filtering
    // Test role-based filtering
    // Test condition evaluation
}

TEST_CASE("LazyLoader manages memory correctly") {
    // Test lazy loading
    // Test cache eviction
    // Test priority preload
}
```

### 2. Integration Tests
- Test with SensorManager + multiple drivers
- Verify UI morphing on config changes
- Test multi-channel consistency

### 3. Performance Tests
- Measure memory usage
- Benchmark filter performance
- Test lazy loading overhead

## 🚀 Migration Plan

### Phase 5.1: Foundation (Week 1)
- [ ] Update manifest schemas
- [ ] Extend process_manifests.py
- [ ] Generate component registry

### Phase 5.2: Runtime (Week 2)
- [ ] Implement UIFilter
- [ ] Create ConditionEvaluator
- [ ] Integrate with ModuleManager

### Phase 5.3: Lazy Loading (Week 3)
- [ ] Implement ComponentFactory
- [ ] Create LazyLoader
- [ ] Add memory management

### Phase 5.4: Integration (Week 4)
- [ ] Update existing modules
- [ ] Test with real hardware
- [ ] Performance optimization

## 🎯 Success Criteria

1. **Performance**: UI updates < 10ms
2. **Memory**: RAM usage reduced by 50%
3. **Compatibility**: All existing modules work
4. **Extensibility**: New drivers auto-integrate
5. **User Experience**: Seamless UI morphing

## 📝 Next Steps

1. Review and approve this implementation guide
2. Create feature branch `phase5-adaptive-ui`
3. Start with Phase 5.1 implementation
4. Weekly progress reviews

---

*Document created: 2025-01-27*  
*Status: Ready for implementation*
