# TODO-006: Hybrid API Contract Implementation Plan

## 🎯 Цілі та Концепція

**Мета**: Створити hybrid API систему з 80% статичними та 20% динамічними компонентами для модульної прошивки ESP32 промислових холодильників.

**Ключові принципи**:
- **80% Static**: Базові API генеруються під час компіляції з manifests
- **20% Dynamic**: Конфігураційно-залежні API генеруються під час boot
- **Restart Pattern**: Зміни конфігурації застосовуються через restart системи
- **Manifest-Driven**: Всі API контракти описуються в module manifests

## 📊 Поточний стан системи

### ✅ Що вже реалізовано:
- `system_contract.h` - централізовані константи для EventBus/SharedState
- `json_rpc_interface.h` - інтерфейс IJsonRpcRegistrar
- `api_dispatcher.h/.cpp` - базовий диспетчер API  
- `module_manifest.json` - детальні описи модулів
- `ui_schema.json` - спрощені UI схеми
- `SensorDriverRegistry` - система реєстрації драйверів
- `Driver::get_ui_schema()` - динамічні схеми від драйверів
- `ui_generator.py` - генерація статичних UI файлів

### ❌ Що потрібно змінити/додати:
- ApiDispatcher не використовує manifests
- Немає автоматичної реєстрації API з manifests
- Немає configuration-driven API generation
- Немає restart pattern для конфігурації
- Немає інтеграції з driver-specific API

## 🏗️ Архітектура Hybrid API System

### 1. Статична частина (Build-time)

```cpp
// Нові файли для створення:
// components/core/include/static_api_registry.h
// components/core/src/static_api_registry.cpp

class StaticApiRegistry {
public:
    // Завантажує базові API з згенерованих даних
    static esp_err_t load_core_apis(ApiDispatcher* dispatcher);
    static esp_err_t load_module_base_apis(ApiDispatcher* dispatcher);
    
    // Генерується з manifests під час компіляції
    static const std::vector<RpcMethodInfo>& get_core_methods();
    static const std::vector<RpcMethodInfo>& get_sensor_base_methods();
    static const std::vector<RpcMethodInfo>& get_actuator_base_methods();
};
```

### 2. Динамічна частина (Boot-time)

```cpp
// Нові файли для створення:
// components/core/include/dynamic_api_builder.h
// components/core/src/dynamic_api_builder.cpp

class DynamicApiBuilder {
public:
    esp_err_t build_configuration_apis(ApiDispatcher* dispatcher);
    
private:
    esp_err_t build_sensor_type_apis(const nlohmann::json& sensor_config);
    esp_err_t build_defrost_type_apis(const nlohmann::json& defrost_config);
    esp_err_t build_scenario_apis(const nlohmann::json& scenario_config);
    
    // Driver-specific API generation
    void register_ds18b20_apis(const std::string& role);
    void register_ntc_apis(const std::string& role);
    void register_gpio_apis(const std::string& role);
};
```

### 3. Configuration Manager з Restart Pattern

```cpp
// Нові файли для створення:
// components/core/include/configuration_manager.h
// components/core/src/configuration_manager.cpp

class ConfigurationManager {
public:
    // Configuration update с restart scheduling
    esp_err_t update_sensor_configuration(const nlohmann::json& new_config);
    esp_err_t update_defrost_configuration(const nlohmann::json& new_config);
    esp_err_t update_scenario_configuration(const nlohmann::json& new_config);
    
    // Restart management
    bool is_restart_required();
    void schedule_restart_for_config_application();
    void mark_restart_required(const std::string& reason);
    
private:
    esp_err_t save_config_to_nvs(const std::string& key, const nlohmann::json& config);
    esp_err_t validate_sensor_config(const nlohmann::json& config);
    void schedule_delayed_restart(uint32_t delay_ms);
};
```

## 🔧 Детальний план реалізації

### Phase 1: Core Infrastructure (3-4 години)

#### 1.1 Створити StaticApiRegistry
```cpp
// Файл: components/core/include/static_api_registry.h
#pragma once
#include "api_dispatcher.h"
#include <vector>

struct RpcMethodInfo {
    std::string method;
    std::string description;
    JsonRpcHandler handler;
    nlohmann::json schema;
};

class StaticApiRegistry {
public:
    static esp_err_t register_all_static_apis(ApiDispatcher* dispatcher);
    
private:
    static esp_err_t register_system_apis(ApiDispatcher* dispatcher);
    static esp_err_t register_sensor_base_apis(ApiDispatcher* dispatcher);
    static esp_err_t register_actuator_base_apis(ApiDispatcher* dispatcher);
    static esp_err_t register_climate_apis(ApiDispatcher* dispatcher);
    static esp_err_t register_network_apis(ApiDispatcher* dispatcher);
};
```

#### 1.2 Створити DynamicApiBuilder
```cpp
// Файл: components/core/include/dynamic_api_builder.h
#pragma once
#include "api_dispatcher.h"
#include "sensor_driver_registry.h"

class DynamicApiBuilder {
public:
    explicit DynamicApiBuilder(ApiDispatcher* dispatcher);
    
    esp_err_t build_all_dynamic_apis();
    
private:
    ApiDispatcher* dispatcher_;
    
    esp_err_t build_sensor_apis();
    esp_err_t build_defrost_apis();
    esp_err_t build_scenario_apis();
    
    // Driver-specific handlers
    JsonRpcHandler create_sensor_set_config_handler(const std::string& role);
    JsonRpcHandler create_sensor_get_config_handler(const std::string& role);
    JsonRpcHandler create_sensor_calibrate_handler(const std::string& role);
};
```

#### 1.3 Створити ConfigurationManager
```cpp
// Файл: components/core/include/configuration_manager.h
#pragma once
#include "esp_err.h"
#include "nlohmann/json.hpp"

class ConfigurationManager {
public:
    static ConfigurationManager& instance();
    
    esp_err_t initialize();
    
    // Configuration updates
    esp_err_t update_sensor_configuration(const nlohmann::json& config);
    esp_err_t update_defrost_configuration(const nlohmann::json& config);
    
    // Restart management
    bool is_restart_required();
    void schedule_restart_if_required();
    
private:
    ConfigurationManager() = default;
    
    bool restart_required_ = false;
    std::string restart_reason_;
    
    esp_err_t save_config_and_mark_restart(const std::string& key, 
                                          const nlohmann::json& config,
                                          const std::string& reason);
};
```

### Phase 2: Enhanced ApiDispatcher (2-3 години)

#### 2.1 Модифікувати ApiDispatcher для підтримки hybrid approach

```cpp
// Файл: components/ui/include/api_dispatcher.h
// Додати методи:

class ApiDispatcher {
public:
    // Existing methods...
    
    // NEW: Hybrid API support
    esp_err_t initialize_hybrid_apis();
    esp_err_t rebuild_dynamic_apis();  // Викликається після зміни конфігурації
    
    // NEW: Method introspection
    nlohmann::json get_available_methods_by_category() const;
    nlohmann::json get_method_schema(const std::string& method) const;
    
    // NEW: Configuration API support
    esp_err_t register_configuration_apis();
    
private:
    bool hybrid_initialized_ = false;
    std::set<std::string> dynamic_methods_;  // Tracking dynamic methods
    
    // NEW: Helper methods
    void clear_dynamic_methods();
    bool is_dynamic_method(const std::string& method) const;
};
```

#### 2.2 Реалізувати Configuration API endpoints

```cpp
// У api_dispatcher.cpp додати:
esp_err_t ApiDispatcher::register_configuration_apis() {
    // Configuration reading
    register_method("config.get_sensors", 
        [](const nlohmann::json& params, nlohmann::json& result) {
            result = load_sensors_config();
            return ESP_OK;
        }, "Get current sensor configuration");
    
    // Configuration updating (with restart requirement)
    register_method("config.update_sensors",
        [](const nlohmann::json& params, nlohmann::json& result) {
            auto& config_mgr = ConfigurationManager::instance();
            esp_err_t ret = config_mgr.update_sensor_configuration(params);
            
            if (ret == ESP_OK) {
                result = {
                    {"success", true},
                    {"restart_required", true},
                    {"message", "Configuration saved. System restart required to apply changes."}
                };
            }
            
            return ret;
        }, "Update sensor configuration (requires restart)");
    
    // Restart management
    register_method("system.restart_for_config",
        [](const nlohmann::json& params, nlohmann::json& result) {
            auto& config_mgr = ConfigurationManager::instance();
            if (config_mgr.is_restart_required()) {
                config_mgr.schedule_restart_if_required();
                result = {"message", "System restart scheduled"};
                return ESP_OK;
            } else {
                result = {"message", "No restart required"};
                return ESP_ERR_INVALID_STATE;
            }
        }, "Restart system to apply configuration changes");
    
    return ESP_OK;
}
```

### Phase 3: Manifest Integration (2-3 години)

#### 3.1 Створити ManifestProcessor

```cpp
// Файл: components/core/include/manifest_processor.h
#pragma once
#include "nlohmann/json.hpp"
#include <string>

class ManifestProcessor {
public:
    static nlohmann::json load_module_manifest(const std::string& module_name);
    static std::vector<std::string> get_available_sensor_types();
    static nlohmann::json get_sensor_type_schema(const std::string& type);
    
    // Runtime manifest processing
    static nlohmann::json build_runtime_api_schema();
    static nlohmann::json build_ui_schema_for_current_config();
    
private:
    static nlohmann::json load_manifest_from_embedded_data(const std::string& module_name);
    static std::string get_manifest_path(const std::string& module_name);
};
```

#### 3.2 Інтегрувати з існуючими driver schemas

```cpp
// У DynamicApiBuilder додати:
esp_err_t DynamicApiBuilder::build_sensor_apis() {
    auto sensor_config = load_sensors_config();
    
    for (const auto& sensor : sensor_config["sensors"]) {
        std::string type = sensor["type"];
        std::string role = sensor["role"];
        
        // Отримуємо UI схему від драйвера
        auto driver = SensorDriverRegistry::instance().create_driver(type);
        if (!driver) continue;
        
        auto ui_schema = driver->get_ui_schema();
        
        // Генеруємо API методи на основі схеми
        generate_sensor_apis_from_schema(role, type, ui_schema);
    }
    
    return ESP_OK;
}

void DynamicApiBuilder::generate_sensor_apis_from_schema(
    const std::string& role, 
    const std::string& type,
    const nlohmann::json& schema) {
    
    std::string base_method = "sensor." + role + ".";
    
    // Базові методи для всіх датчиків
    dispatcher_->register_method(base_method + "get_value",
        create_sensor_get_value_handler(role));
    dispatcher_->register_method(base_method + "get_diagnostics", 
        create_sensor_get_diagnostics_handler(role));
    
    // Генеруємо методи на основі properties в UI схемі
    if (schema.contains("properties")) {
        for (const auto& [prop, definition] : schema["properties"].items()) {
            // Створюємо set/get методи для кожного property
            dispatcher_->register_method(base_method + "set_" + prop,
                create_sensor_set_property_handler(role, prop, definition));
            dispatcher_->register_method(base_method + "get_" + prop,
                create_sensor_get_property_handler(role, prop));
        }
    }
}
```

### Phase 4: UI Integration (2 години)

#### 4.1 Розширити ui_generator.py для hybrid approach

```python
# Файл: tools/ui_generator.py
# Додати клас:

class HybridUIGenerator(UIGenerator):
    def __init__(self, components_dir='components', output_dir='main/generated'):
        super().__init__(components_dir, output_dir)
        
    def generate_adaptive_ui_schema(self):
        """Generate UI schema that adapts to runtime configuration"""
        
        # 1. Генеруємо статичну частину з manifests
        static_schema = self.generate_static_ui_from_manifests()
        
        # 2. Генеруємо шаблони для динамічної частини
        dynamic_templates = self.generate_dynamic_ui_templates()
        
        # 3. Створюємо adaptive UI header
        self.generate_adaptive_ui_header(static_schema, dynamic_templates)
        
    def generate_dynamic_ui_templates(self):
        """Generate UI templates for each possible configuration"""
        templates = {}
        
        # Sensor type templates
        for module_dir in self.components_dir.glob("*/"):
            if (module_dir / "module_manifest.json").exists():
                manifest = self.load_manifest(module_dir / "module_manifest.json")
                
                if "ui_interfaces" in manifest:
                    templates[module_dir.name] = manifest["ui_interfaces"]
                    
        return templates
```

#### 4.2 Створити adaptive UI генерацію

```cpp
// Файл: components/ui/include/adaptive_ui_generator.h
#pragma once
#include "nlohmann/json.hpp"

class AdaptiveUIGenerator {
public:
    // Генерування UI на основі поточної конфігурації
    static nlohmann::json generate_sensor_configuration_ui();
    static nlohmann::json generate_defrost_configuration_ui();
    static nlohmann::json generate_current_controls();
    
    // UI для зміни конфігурації з попередженням про restart
    static nlohmann::json generate_configuration_change_ui();
    
private:
    static nlohmann::json create_sensor_type_selector();
    static nlohmann::json create_conditional_properties(const std::string& sensor_type);
    static nlohmann::json add_restart_warning_to_schema(const nlohmann::json& schema);
};
```

### Phase 5: Integration та Testing (1-2 години)

#### 5.1 Модифікувати main initialization

```cpp
// У main/main.cpp модифікувати:
extern "C" void app_main() {
    // Existing initialization...
    
    // Initialize configuration manager
    ConfigurationManager::instance().initialize();
    
    // Initialize API system with hybrid approach
    api_dispatcher.initialize_hybrid_apis();
    
    // Check if restart was required for configuration
    if (ConfigurationManager::instance().is_restart_required()) {
        ESP_LOGI(TAG, "Configuration changes applied after restart");
        // Clear restart flag
    }
    
    // Existing code...
}
```

#### 5.2 Додати конфігураційні RPC endpoints

```cpp
// Реєструвати нові методи:
{
    "config.get_available_sensor_types": "Get list of compiled sensor drivers",
    "config.get_sensor_schema": "Get UI schema for specific sensor type", 
    "config.validate_sensor_config": "Validate sensor configuration",
    "config.get_restart_status": "Check if restart is required",
    "system.get_api_documentation": "Get current API documentation"
}
```

## 📁 Файли для створення/модифікації

### Нові файли:
```
components/core/include/static_api_registry.h
components/core/src/static_api_registry.cpp
components/core/include/dynamic_api_builder.h  
components/core/src/dynamic_api_builder.cpp
components/core/include/configuration_manager.h
components/core/src/configuration_manager.cpp
components/core/include/manifest_processor.h
components/core/src/manifest_processor.cpp
components/ui/include/adaptive_ui_generator.h
components/ui/src/adaptive_ui_generator.cpp
```

### Файли для модифікації:
```
components/ui/include/api_dispatcher.h          - додати hybrid support
components/ui/src/api_dispatcher.cpp            - реалізувати hybrid methods
tools/ui_generator.py                           - додати adaptive generation
main/main.cpp                                   - integration points
```

## 🧪 Критерії готовності TODO-006

### ✅ Functional Requirements:
- [ ] Статичні API (80%) генеруються з manifests під час компіляції
- [ ] Динамічні API (20%) генеруються на основі конфігурації при boot
- [ ] Configuration changes працюють через restart pattern
- [ ] Driver-specific API автоматично генерується з UI schemas
- [ ] Validation працює для всіх API запитів
- [ ] UI адаптується до поточної конфігурації

### ✅ Technical Requirements:
- [ ] Memory usage < 20KB для API metadata
- [ ] API response time < 100ms
- [ ] Configuration change + restart < 10 seconds  
- [ ] Всі API документуються автоматично
- [ ] Backwards compatibility з існуючими API

### ✅ Integration Requirements:
- [ ] Працює з існуючими SharedState/EventBus
- [ ] Інтегрується з sensor/actuator drivers
- [ ] WebUI автоматично оновлюється
- [ ] MQTT topics адаптуються до конфігурації
- [ ] Logging та diagnostics працюють

## 🚀 План виконання

**Тиждень 1:**
- Phase 1: Core Infrastructure (3-4 години)
- Phase 2: Enhanced ApiDispatcher (2-3 години)

**Тиждень 2:**  
- Phase 3: Manifest Integration (2-3 години)
- Phase 4: UI Integration (2 години)
- Phase 5: Integration та Testing (1-2 години)

**Загальний час:** 10-14 годин

## 💡 Ключові переваги реалізації

1. **Ефективність**: 80% API статичні - мінімальний runtime overhead
2. **Гнучкість**: 20% динамічних API адаптуються до конфігурації  
3. **Простота**: Restart pattern простіший за hot-reload
4. **Надійність**: Atomic configuration changes
5. **Maintainability**: Manifest-driven approach
6. **Scalability**: Легко додавати нові типи драйверів
7. **User Experience**: Зрозумілий workflow для техніків

---

*TODO-006: Від статичних контрактів до adaptive API ecosystem! 🚀*