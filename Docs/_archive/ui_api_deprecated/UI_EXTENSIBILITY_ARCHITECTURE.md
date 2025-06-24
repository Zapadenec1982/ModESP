# Розширювана архітектура UI/API для ModESP

## Концепція: UI як незалежні модулі-адаптери

### Архітектура

```
┌─────────────────────────────────────────────────────────────┐
│                     Business Logic Modules                   │
│  (SensorModule, ActuatorModule, ClimateControl, etc.)       │
└─────────────┬──────────────────────────────┬────────────────┘
              │                              │
              ▼                              ▼
    ┌─────────────────┐            ┌──────────────────┐
    │ Module Registry │            │ UI Registry      │
    │ + Metadata      │            │ + UI Schemas     │
    └────────┬────────┘            └─────────┬────────┘
             │                               │
             └───────────┬───────────────────┘
                         │
    ┌────────────────────┼────────────────────────────┐
    ▼                    ▼                            ▼
┌───────────┐    ┌──────────────┐    ┌─────────────────┐
│WebUIModule│    │ MQTTModule   │    │TelegramModule   │
└───────────┘    └──────────────┘    └─────────────────┘
    ▼                    ▼                            ▼
┌───────────┐    ┌──────────────┐    ┌─────────────────┐
│LCDUIModule│    │ModbusModule  │    │MobileAPIModule  │
└───────────┘    └──────────────┘    └─────────────────┘
```

## Ключові компоненти

### 1. UI Metadata в модулях

Кожен бізнес-модуль описує свій UI через метадані:

```cpp
class SensorModule : public BaseModule {
public:
    // Новий метод для UI метаданих
    nlohmann::json get_ui_schema() const override {
        return {
            {"module", get_name()},
            {"version", "1.0"},
            {"controls", {
                {
                    {"id", "sensor_list"},
                    {"type", "list"},
                    {"label", "Sensors"},
                    {"read_method", "sensor.get_all"},
                    {"items", {
                        {
                            {"id", "temperature"},
                            {"type", "gauge"},
                            {"label", "Temperature"},
                            {"unit", "°C"},
                            {"min", -40},
                            {"max", 60},
                            {"read_method", "sensor.get_value"},
                            {"params", {{"sensor_id", "temp_1"}}}
                        }
                    }}
                },
                {
                    {"id", "refresh_rate"},
                    {"type", "select"},
                    {"label", "Update Rate"},
                    {"options", {
                        {"1", "1 second"},
                        {"5", "5 seconds"},
                        {"10", "10 seconds"}
                    }},
                    {"read_method", "sensor.get_config"},
                    {"write_method", "sensor.set_config"}
                }
            }},
            {"telemetry", {
                {"temperature", {
                    {"source", "state.sensor.temperature"},
                    {"interval", 60}
                }}
            }},
            {"alarms", {
                {"high_temp", {
                    {"condition", "temperature > 40"},
                    {"message", "High temperature alarm!"},
                    {"severity", "warning"}
                }}
            }}
        };
    }
    
    // Метод для отримання можливостей модуля
    nlohmann::json get_capabilities() const override {
        return {
            {"features", {"read", "write", "telemetry", "alarms"}},
            {"protocols", {"rest", "mqtt", "modbus"}},
            {"update_rate_ms", 1000}
        };
    }
};
```

### 2. Базовий клас для UI адаптерів

```cpp
/**
 * @brief Base class for all UI adapters
 */
class UIAdapterBase : public BaseModule {
protected:
    // Registry of all modules' UI schemas
    std::map<std::string, nlohmann::json> ui_schemas_;
    
    // Common functionality
    virtual void discover_modules();
    virtual void register_ui_handlers() = 0;
    virtual void handle_read_request(const std::string& method, 
                                   const nlohmann::json& params,
                                   nlohmann::json& response);
    virtual void handle_write_request(const std::string& method,
                                    const nlohmann::json& params);
    
public:
    esp_err_t init() override {
        discover_modules();
        register_ui_handlers();
        return ESP_OK;
    }
};
```

### 3. Приклади UI адаптерів

#### MQTT Adapter
```cpp
class MQTTUIAdapter : public UIAdapterBase {
private:
    esp_mqtt_client_handle_t mqtt_client_;
    
    void register_ui_handlers() override {
        // Автоматично створює MQTT топіки з UI схем
        for (const auto& [module, schema] : ui_schemas_) {
            if (schema.contains("telemetry")) {
                for (const auto& [key, config] : schema["telemetry"].items()) {
                    std::string topic = "modesp/" + module + "/" + key;
                    subscribe_telemetry(topic, config["source"]);
                }
            }
        }
    }
    
    void subscribe_telemetry(const std::string& topic, const std::string& source) {
        SharedState::subscribe(source, [this, topic](const auto& value) {
            publish_mqtt(topic, value);
        });
    }
};
```

#### LCD UI Adapter
```cpp
class LCDUIAdapter : public UIAdapterBase {
private:
    struct Page {
        std::string title;
        std::vector<std::string> data_sources;
        std::function<void()> render;
    };
    std::vector<Page> pages_;
    
    void register_ui_handlers() override {
        // Генерує сторінки LCD з UI схем
        for (const auto& [module, schema] : ui_schemas_) {
            if (schema.contains("controls")) {
                create_lcd_page(module, schema["controls"]);
            }
        }
    }
    
    void create_lcd_page(const std::string& module, const nlohmann::json& controls) {
        Page page;
        page.title = module;
        
        for (const auto& control : controls) {
            if (control["type"] == "gauge" || control["type"] == "value") {
                page.data_sources.push_back(control["read_method"]);
            }
        }
        
        pages_.push_back(page);
    }
};
```

#### Telegram Bot Adapter
```cpp
class TelegramUIAdapter : public UIAdapterBase {
private:
    void register_ui_handlers() override {
        // Створює команди Telegram з UI схем
        for (const auto& [module, schema] : ui_schemas_) {
            register_module_commands(module, schema);
        }
    }
    
    void register_module_commands(const std::string& module, const nlohmann::json& schema) {
        // /sensors - показати всі датчики
        bot_.register_command("/" + module, [this, module](const std::string& chat_id) {
            send_module_status(chat_id, module);
        });
        
        // Автоматичні алерти
        if (schema.contains("alarms")) {
            setup_alarm_notifications(module, schema["alarms"]);
        }
    }
};
```

### 4. Система автореєстрації

```cpp
// В ModuleRegistry
class ModuleRegistry {
public:
    struct ModuleInfo {
        BaseModule* instance;
        nlohmann::json ui_schema;
        nlohmann::json capabilities;
    };
    
    static void register_module(BaseModule* module) {
        ModuleInfo info;
        info.instance = module;
        info.ui_schema = module->get_ui_schema();
        info.capabilities = module->get_capabilities();
        
        modules_[module->get_name()] = info;
        
        // Повідомляємо всі UI адаптери про новий модуль
        EventBus::publish("module.registered", {
            {"name", module->get_name()},
            {"schema", info.ui_schema}
        });
    }
};
```

### 5. Конфігурація для різних UI

```json
// ui_config.json
{
    "ui_adapters": {
        "web": {
            "enabled": true,
            "port": 80,
            "update_interval_ms": 1000
        },
        "mqtt": {
            "enabled": true,
            "broker": "mqtt://broker.hivemq.com",
            "base_topic": "modesp/device123",
            "telemetry_interval_s": 60
        },
        "telegram": {
            "enabled": false,
            "bot_token": "YOUR_BOT_TOKEN",
            "allowed_users": [123456789]
        },
        "lcd": {
            "enabled": true,
            "type": "i2c_20x4",
            "pages": "auto",
            "backlight_timeout_s": 30
        },
        "modbus": {
            "enabled": false,
            "mode": "rtu",
            "slave_id": 1,
            "register_map": "auto"
        }
    }
}
```

## Переваги архітектури

### 1. Повна незалежність UI
- Кожен UI адаптер - окремий модуль
- Можна включати/виключати через конфігурацію
- Не впливають один на одного

### 2. Автоматична інтеграція
- Новий модуль автоматично з'являється у всіх UI
- Не потрібно змінювати код UI адаптерів
- Працює через метадані

### 3. Гнучкість представлення
- Кожен UI адаптер інтерпретує схему по-своєму
- LCD показує тільки основні дані
- Web UI створює повноцінний інтерфейс
- MQTT публікує телеметрію

### 4. Легке додавання нових протоколів
```cpp
// Новий протокол - просто новий адаптер
class CoAPUIAdapter : public UIAdapterBase {
    void register_ui_handlers() override {
        // Реалізація CoAP endpoints
    }
};
```

## Приклад додавання нового модуля

```cpp
// Новий модуль автоматично отримує UI
class PressureControlModule : public BaseModule {
public:
    nlohmann::json get_ui_schema() const override {
        return {
            {"module", "pressure_control"},
            {"controls", {
                {
                    {"id", "pressure"},
                    {"type", "gauge"},
                    {"label", "System Pressure"},
                    {"unit", "bar"},
                    {"min", 0},
                    {"max", 10},
                    {"read_method", "pressure.get_current"}
                },
                {
                    {"id", "setpoint"},
                    {"type", "number"},
                    {"label", "Pressure Setpoint"},
                    {"unit", "bar"},
                    {"min", 0},
                    {"max", 8},
                    {"step", 0.1},
                    {"read_method", "pressure.get_setpoint"},
                    {"write_method", "pressure.set_setpoint"}
                }
            }}
        };
    }
};
```

Після реєстрації цього модуля:
- **Web UI**: автоматично створить сторінку з gauge та input
- **MQTT**: почне публікувати `modesp/device/pressure`
- **LCD**: додасть сторінку з показаннями тиску
- **Telegram**: створить команду `/pressure_control`
- **Modbus**: виділить регістри для pressure та setpoint

## Висновок

Ця архітектура забезпечує:
1. **Автоматичну** інтеграцію нових модулів у всі UI
2. **Незалежність** різних UI один від одного
3. **Гнучкість** у додаванні нових протоколів
4. **Мінімальні** зміни при розширенні системи