# Phase 5: Technical Architecture Specification

## 🏗️ System Architecture Overview

### Three-Layer Architecture
```
┌─────────────────────────────────────┐
│ Layer 1: Build-time Generation      │
│ • All possible UI components        │
│ • Component metadata & dependencies │
│ • Validation & optimization         │
└────────────────┬────────────────────┘
                 │
┌────────────────▼────────────────────┐
│ Layer 2: Runtime Filtering          │
│ • Configuration-based filtering     │
│ • Role-based access control        │
│ • Dynamic condition evaluation     │
└────────────────┬────────────────────┘
                 │
┌────────────────▼────────────────────┐
│ Layer 3: Lazy Loading              │
│ • On-demand component loading      │
│ • Priority preloading              │
│ • Memory management                │
└─────────────────────────────────────┘
```

## 📐 Detailed Component Design

### 1. Build-time Component Generator

#### Input Processing
```python
class ComponentGenerator:
    def __init__(self):
        self.components = []
        self.metadata = {}
        
    def process_manager_manifest(self, manifest):
        """Extract UI components from manager manifest"""
        for component in manifest['ui_components']['all_possible']:
            self.generate_component(component)
            
    def process_driver_manifest(self, manifest):
        """Extract UI extensions from driver manifest"""
        for extension in manifest['ui_extensions']:
            self.generate_extension(extension)
```

#### Output Generation
```cpp
// generated_ui_components.h
namespace ModESP::UI {

// Component metadata stored in PROGMEM
const ComponentMetadata PROGMEM component_metadata[] = {
    {
        .id = "sensor_type_selector",
        .type = ComponentType::DROPDOWN,
        .size = sizeof(DropdownComponent),
        .condition = "always",
        .access_level = AccessLevel::USER,
        .priority = Priority::HIGH,
        .factory_index = 0
    },
    {
        .id = "ds18b20_resolution",
        .type = ComponentType::SLIDER,
        .size = sizeof(SliderComponent),
        .condition = "config.sensor.type == 'DS18B20'",
        .access_level = AccessLevel::TECHNICIAN,
        .priority = Priority::MEDIUM,
        .factory_index = 1
    }
    // ... all other components
};

// Component factories array
using ComponentFactory = Component* (*)();
const ComponentFactory component_factories[] PROGMEM = {
    []() -> Component* { return new SensorTypeSelectorComponent(); },
    []() -> Component* { return new DS18B20ResolutionComponent(); },
    // ... all factories
};

} // namespace
```

### 2. Smart Filter Engine

#### Condition Evaluator
```cpp
class ConditionEvaluator {
private:
    struct ConditionAST {
        enum Type { ALWAYS, EQUALS, NOT_EQUALS, AND, OR, HAS_FEATURE };
        Type type;
        std::string left;
        std::string right;
        std::unique_ptr<ConditionAST> left_child;
        std::unique_ptr<ConditionAST> right_child;
    };
    
    std::unique_ptr<ConditionAST> ast;
    
public:
    ConditionEvaluator(const char* condition) {
        ast = parse(condition);
    }
    
    bool evaluate(const Config& config, const Context& context) {
        return evaluate_ast(ast.get(), config, context);
    }
    
private:
    bool evaluate_ast(ConditionAST* node, const Config& config, const Context& context) {
        switch(node->type) {
            case ConditionAST::ALWAYS:
                return true;
                
            case ConditionAST::EQUALS:
                return get_config_value(node->left, config) == node->right;
                
            case ConditionAST::HAS_FEATURE:
                return context.hasFeature(node->left);
                
            case ConditionAST::AND:
                return evaluate_ast(node->left_child.get(), config, context) &&
                       evaluate_ast(node->right_child.get(), config, context);
                
            // ... other cases
        }
    }
};
```

#### Optimized Filter
```cpp
class AdaptiveUIFilter {
private:
    // Cache for condition evaluation results
    mutable std::unordered_map<size_t, bool> condition_cache;
    
public:
    std::vector<ComponentMetadata*> filter(
        const std::vector<ComponentMetadata*>& all_components,
        const Config& config,
        UserRole role,
        const Context& context
    ) {
        std::vector<ComponentMetadata*> visible;
        visible.reserve(all_components.size() / 4); // Optimize allocation
        
        // Clear cache if config changed
        if (config.hasChanged()) {
            condition_cache.clear();
        }
        
        for (auto* component : all_components) {
            // Check access level first (fast)
            if (component->access_level > role) continue;
            
            // Check condition (potentially cached)
            size_t condition_hash = hash_condition(component->condition, config);
            auto cache_it = condition_cache.find(condition_hash);
            
            bool condition_result;
            if (cache_it != condition_cache.end()) {
                condition_result = cache_it->second;
            } else {
                ConditionEvaluator evaluator(component->condition);
                condition_result = evaluator.evaluate(config, context);
                condition_cache[condition_hash] = condition_result;
            }
            
            if (condition_result) {
                visible.push_back(component);
            }
        }
        
        return visible;
    }
};
```

### 3. Lazy Loading System

#### Component Loader
```cpp
class LazyComponentLoader {
private:
    struct ComponentEntry {
        std::unique_ptr<Component> instance;
        size_t last_access_time;
        bool is_priority;
    };
    
    std::unordered_map<std::string, ComponentEntry> loaded_components;
    std::set<std::string> priority_components;
    size_t total_memory_usage = 0;
    static constexpr size_t MAX_MEMORY_USAGE = 50 * 1024; // 50KB limit
    
public:
    void registerPriority(const std::vector<std::string>& priority_ids) {
        priority_components.insert(priority_ids.begin(), priority_ids.end());
    }
    
    void preloadPriority() {
        for (const auto& id : priority_components) {
            load(id.c_str());
        }
    }
    
    Component* load(const char* id) {
        auto it = loaded_components.find(id);
        if (it != loaded_components.end()) {
            it->second.last_access_time = esp_timer_get_time();
            return it->second.instance.get();
        }
        
        // Find component metadata
        const ComponentMetadata* metadata = find_metadata(id);
        if (!metadata) return nullptr;
        
        // Check memory limit
        if (total_memory_usage + metadata->size > MAX_MEMORY_USAGE) {
            evict_lru_components(metadata->size);
        }
        
        // Create component
        auto factory = pgm_read_ptr(&component_factories[metadata->factory_index]);
        auto component = std::unique_ptr<Component>(factory());
        
        // Store and track memory
        ComponentEntry entry{
            .instance = std::move(component),
            .last_access_time = esp_timer_get_time(),
            .is_priority = priority_components.count(id) > 0
        };
        
        total_memory_usage += metadata->size;
        auto* ptr = entry.instance.get();
        loaded_components[id] = std::move(entry);
        
        ESP_LOGI(TAG, "Loaded component %s (size: %d, total: %d)", 
                 id, metadata->size, total_memory_usage);
        
        return ptr;
    }
    
private:
    void evict_lru_components(size_t needed_space) {
        std::vector<std::pair<std::string, size_t>> candidates;
        
        for (const auto& [id, entry] : loaded_components) {
            if (!entry.is_priority) {
                candidates.emplace_back(id, entry.last_access_time);
            }
        }
        
        // Sort by last access time
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) {
                      return a.second < b.second;
                  });
        
        // Evict until we have enough space
        for (const auto& [id, _] : candidates) {
            if (total_memory_usage + needed_space <= MAX_MEMORY_USAGE) break;
            
            auto it = loaded_components.find(id);
            if (it != loaded_components.end()) {
                const ComponentMetadata* metadata = find_metadata(id.c_str());
                total_memory_usage -= metadata->size;
                loaded_components.erase(it);
                
                ESP_LOGI(TAG, "Evicted component %s", id.c_str());
            }
        }
    }
};
```

## 🔄 Integration Points

### ModuleManager Enhancement
```cpp
class ModuleManager {
private:
    // New members for adaptive UI
    std::unique_ptr<AdaptiveUIFilter> ui_filter;
    std::unique_ptr<LazyComponentLoader> component_loader;
    std::vector<ComponentMetadata*> all_ui_components;
    
public:
    esp_err_t init() {
        // ... existing init code ...
        
        // Initialize adaptive UI system
        if (Config::isAdaptiveUIEnabled()) {
            ui_filter = std::make_unique<AdaptiveUIFilter>();
            component_loader = std::make_unique<LazyComponentLoader>();
            
            // Load component metadata from PROGMEM
            load_component_metadata();
            
            // Register priority components
            auto priority_ids = Config::getPriorityUIComponents();
            component_loader->registerPriority(priority_ids);
        }
        
        return ESP_OK;
    }
    
    std::vector<Component*> getVisibleUIComponents(UserRole role) {
        if (!Config::isAdaptiveUIEnabled()) {
            return get_legacy_ui_components();
        }
        
        // Filter components based on current state
        auto visible_metadata = ui_filter->filter(
            all_ui_components,
            Config::getCurrent(),
            role,
            Context::getCurrent()
        );
        
        // Lazy load visible components
        std::vector<Component*> components;
        components.reserve(visible_metadata.size());
        
        for (auto* metadata : visible_metadata) {
            if (auto* component = component_loader->load(metadata->id)) {
                components.push_back(component);
            }
        }
        
        return components;
    }
};
```

## 📊 Performance Optimizations

### Memory Layout
```cpp
// Optimize struct packing
struct ComponentMetadata {
    const char* id;              // 4 bytes
    ComponentType type : 8;      // 1 byte
    AccessLevel access_level : 8;// 1 byte  
    Priority priority : 8;       // 1 byte
    uint8_t factory_index;       // 1 byte
    uint16_t size;              // 2 bytes
    const char* condition;       // 4 bytes
} __attribute__((packed));      // Total: 14 bytes
```

### Caching Strategy
1. **Condition Results**: Cache evaluation results per config state
2. **Component Instances**: LRU eviction for non-priority components
3. **Metadata Access**: Keep frequently accessed metadata in RAM

### Build-time Optimizations
1. **Dead Code Elimination**: Remove unused component code
2. **Constant Folding**: Pre-evaluate static conditions
3. **String Interning**: Share common strings in PROGMEM

## 🧪 Testing Strategy

### Unit Tests
```cpp
TEST_CASE("AdaptiveUIFilter filters correctly", "[adaptive-ui]") {
    AdaptiveUIFilter filter;
    Config config;
    config["sensor"]["type"] = "DS18B20";
    
    auto visible = filter.filter(all_components, config, UserRole::TECHNICIAN);
    
    // Should include DS18B20-specific components
    REQUIRE(contains_component(visible, "ds18b20_resolution"));
    REQUIRE(contains_component(visible, "ds18b20_parasitic_power"));
    
    // Should not include NTC-specific components
    REQUIRE_FALSE(contains_component(visible, "ntc_resistance_ref"));
}

TEST_CASE("LazyLoader respects memory limit", "[adaptive-ui]") {
    LazyComponentLoader loader;
    loader.setMemoryLimit(10 * 1024); // 10KB
    
    // Load components until limit
    for (int i = 0; i < 20; i++) {
        loader.load(test_components[i].id);
    }
    
    REQUIRE(loader.getMemoryUsage() <= 10 * 1024);
}
```

### Integration Tests
1. **End-to-end UI generation** from manifest to rendered output
2. **Performance benchmarks** comparing with existing system
3. **Memory stress tests** with many components
4. **Multi-channel consistency** tests

## 🚀 Migration Path

### Phase 1: Coexistence
- Both systems run in parallel
- Feature flag to switch between them
- Gradual module migration

### Phase 2: Migration
- Convert modules one by one
- Validate performance at each step
- Maintain backward compatibility

### Phase 3: Deprecation
- Remove old system
- Clean up legacy code
- Optimize for new architecture