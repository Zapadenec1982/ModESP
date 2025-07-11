# Adaptive UI Architecture: Build-time + Runtime + Lazy Loading

## 🎯 Концепція

**Тришарова архітектура** для максимально ефективної генерації адаптивного UI:

1. **Build-time**: Генерація ВСІХ можливих компонентів
2. **Runtime**: Smart фільтрація за конфігурацією та роллю
3. **Lazy Loading**: Завантаження тільки потрібних компонентів

## 🏗️ Архітектура системи

### Layer 1: Build-time Generation
```
Manager Manifest + Driver Manifests
           ↓
    Component Generator
           ↓
┌─────────────────────────┐
│   ALL Possible UI       │
│   ├── DS18B20 Components│
│   ├── NTC Components    │
│   ├── GPIO Components   │
│   ├── Admin Components  │
│   └── User Components   │
└─────────────────────────┘
```

### Layer 2: Runtime Filtering
```cpp
class UIFilter {
    FilterCriteria criteria;
    
    std::vector<Component*> filter(
        const std::vector<Component*>& all_components,
        const Config& config,
        UserRole role
    ) {
        std::vector<Component*> visible;
        
        for (auto* comp : all_components) {
            if (comp->isApplicable(config) && 
                comp->hasAccess(role)) {
                visible.push_back(comp);
            }
        }
        
        return visible;
    }
};
```

### Layer 3: Lazy Loading
```cpp
class LazyComponentLoader {
private:
    using ComponentFactory = std::function<std::unique_ptr<Component>()>;
    
    std::unordered_map<std::string, ComponentFactory> factories;
    std::unordered_map<std::string, std::unique_ptr<Component>> loaded_cache;
    std::set<std::string> priority_components;

public:
    // Завантажуємо тільки при першому зверненні
    Component* get(const std::string& name) {
        if (loaded_cache.find(name) == loaded_cache.end()) {
            loaded_cache[name] = factories[name]();
        }
        return loaded_cache[name].get();
    }
    
    // Preload критичних компонентів
    void preloadPriority() {
        for (const auto& name : priority_components) {
            get(name); // Форсуємо завантаження
        }
    }
};
```

## 📋 Manifest Architecture

### Manager Manifest з повним описом
```json
{
  "manager": "SensorManager",
  "ui_components": {
    "all_possible": [
      {
        "id": "sensor_type_selector",
        "type": "dropdown",
        "applicability": ["always"],
        "access_level": "user",
        "priority": "high"
      },
      {
        "id": "ds18b20_resolution",
        "type": "slider",
        "applicability": ["sensor_type == 'DS18B20'"],
        "access_level": "technician",
        "priority": "medium"
      },
      {
        "id": "factory_reset_sensor",
        "type": "button",
        "applicability": ["sensor_type != 'none'"],
        "access_level": "supervisor",
        "priority": "low"
      }
    ]
  }
}
```

### Driver Manifest з conditional UI
```json
{
  "driver": "DS18B20Driver",
  "ui_extensions": [
    {
      "id": "ds18b20_parasitic_power",
      "condition": "config.sensor_type == 'DS18B20'",
      "inject_into": "sensor_config_panel",
      "component": {
        "type": "toggle",
        "label": "Parasitic Power",
        "access_level": "technician"
      }
    }
  ]
}
```

## 🔄 Runtime Process Flow

### 1. System Startup
```cpp
void SystemUI::initialize() {
    // 1. Load всі згенеровані компоненти (metadata)
    component_registry.loadAllMetadata();
    
    // 2. Preload критичних компонентів
    lazy_loader.preloadPriority();
    
    // 3. Apply initial filter
    updateVisibleComponents();
}
```

### 2. Configuration Change
```cpp
void SystemUI::onConfigChange(const Config& new_config) {
    // 1. Refilter components
    auto visible = filter.apply(all_components, new_config, current_role);
    
    // 2. Lazy load нових потрібних компонентів
    for (auto* comp : visible) {
        if (!comp->isLoaded()) {
            lazy_loader.get(comp->getId());
        }
    }
    
    // 3. Update UI
    ui_renderer.render(visible);
}
```

### 3. Role Change
```cpp
void SystemUI::onRoleChange(UserRole new_role) {
    current_role = new_role;
    
    // Refilter з новою роллю
    updateVisibleComponents();
}
```

## 🎮 Multi-channel Implementation

### Unified Component → Channel Adapters
```cpp
class Component {
    virtual void renderLCD(LCDRenderer& renderer) = 0;
    virtual void renderWeb(WebRenderer& renderer) = 0;
    virtual void renderMQTT(MQTTRenderer& renderer) = 0;
    virtual void renderTelegram(TelegramRenderer& renderer) = 0;
};

class DS18B20ResolutionComponent : public Component {
    void renderLCD(LCDRenderer& renderer) override {
        renderer.showSlider("Resolution", value, 9, 12);
    }
    
    void renderWeb(WebRenderer& renderer) override {
        renderer.addSlider("ds18b20_resolution", {
            .min = 9, .max = 12, .value = value,
            .label = "Resolution (bits)"
        });
    }
    
    void renderMQTT(MQTTRenderer& renderer) override {
        renderer.publishConfig("sensor/ds18b20/resolution", {
            .type = "number", .min = 9, .max = 12
        });
    }
};
```

## 🚀 Performance Benefits

### 1. **Memory Efficiency**
```
Traditional: All UI loaded = 100% RAM usage
Lazy Loading: Only visible UI = 20-40% RAM usage
```

### 2. **Startup Speed**
```
Traditional: Load everything = 2-5 seconds
Priority + Lazy: Critical first = 0.5-1 second
```

### 3. **Runtime Performance**
```
Build-time generation = 0ms runtime generation
Filtering = O(n) where n = total components
Lazy loading = O(1) amortized access
```

## 📊 Переваги Adaptive UI архітектури

| Аспект | Традиційний підхід | Adaptive UI |
|--------|-------------------|------------|
| **Складність** | Висока (подвійні системи) | Середня (єдина система) |
| **Продуктивність** | Змішана | Консистентна |
| **Використання пам'яті** | Фіксовано високе | Адаптивне |
| **Тестування** | Складне (комбінації) | Легке (детерміністичне) |
| **Підтримка** | Складна | Чиста |
| **Runtime помилки** | Можливі | Виключені |

## 🔧 Implementation Phases

### Phase 1: Component Generator
```bash
# Build system
python generate_all_components.py
├── Scan all manager manifests
├── Discover all driver manifests  
├── Generate component classes
├── Create component registry
└── Build filter metadata
```

### Phase 2: Filter Engine
```cpp
class SmartFilter {
    bool evaluateCondition(const std::string& condition, 
                          const Config& config);
    bool hasAccess(UserRole role, const std::string& access_level);
    std::vector<Component*> apply(const FilterCriteria& criteria);
};
```

### Phase 3: Lazy Loader
```cpp
class LazyLoader {
    void registerFactory(const std::string& id, ComponentFactory factory);
    Component* get(const std::string& id);
    void preload(const std::vector<std::string>& priority_ids);
    void unload(const std::string& id); // Memory management
};
```

## 🎯 Real-world Example

### Scenario: Додаємо новий NTC сенсор
```json
// ntc_driver_manifest.json
{
  "driver": "NTCDriver",
  "ui_components": [
    {
      "id": "ntc_resistance_ref",
      "condition": "config.sensor_type == 'NTC'",
      "component": {
        "type": "number_input",
        "label": "Reference Resistance (Ω)",
        "min": 1000, "max": 100000,
        "access_level": "technician"
      }
    }
  ]
}
```

### Build-time Generation:
```cpp
// Auto-generated
class NTCResistanceComponent : public Component {
    bool isApplicable(const Config& config) override {
        return config.sensor_type == "NTC";
    }
    
    bool hasAccess(UserRole role) override {
        return role >= UserRole::TECHNICIAN;
    }
};
```

### Runtime Usage:
```cpp
// User вибрав NTC в конфігурації
config.sensor_type = "NTC";

// System автоматично:
// 1. Filter показує NTC компоненти
// 2. Lazy loader завантажує тільки потрібні
// 3. UI морфінг показує resistance input
// 4. Всі канали (LCD/Web/MQTT) синхронізовані
```

## 🏆 Висновок

**Build-time + Runtime filtering + Lazy loading** = **Optimal approach!**

### Переваги:
- ✅ **Zero runtime generation** - все готове на build-time
- ✅ **Smart memory usage** - lazy loading економить RAM
- ✅ **Deterministic behavior** - filtering передбачуваний
- ✅ **Easy testing** - можна тестувати всі комбінації
- ✅ **Clean architecture** - unified approach
- ✅ **Scalable** - легко додавати нові компоненти

**Це справді революційний підхід для embedded систем!** 🎉 