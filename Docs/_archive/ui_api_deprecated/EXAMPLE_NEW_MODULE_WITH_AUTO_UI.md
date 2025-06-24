# Приклад: Додавання нового модуля з автоматичним UI

Цей приклад показує, як додати новий модуль "Контроль вологості" і автоматично отримати UI для всіх протоколів.

## 1. Створення модуля

```cpp
// humidity_control_module.h
class HumidityControlModule : public BaseModule {
private:
    float current_humidity_ = 0.0;
    float target_humidity_ = 60.0;
    bool dehumidifier_active_ = false;
    bool humidifier_active_ = false;
    
public:
    const char* get_name() const override { return "HumidityControl"; }
    
    // Опис UI схеми
    nlohmann::json get_ui_schema() const override {
        return {
            {"module", get_name()},
            {"version", "1.0"},
            {"icon", "water-drop"},  // Іконка для UI
            {"controls", {
                // Відображення поточної вологості
                {
                    {"id", "current_humidity"},
                    {"type", "gauge"},
                    {"label", "Current Humidity"},
                    {"unit", "%"},
                    {"min", 0},
                    {"max", 100},
                    {"thresholds", {
                        {"low", 40},
                        {"optimal", {50, 70}},
                        {"high", 80}
                    }},
                    {"read_method", "humidity.get_current"}
                },
                // Налаштування цільової вологості
                {
                    {"id", "target_humidity"},
                    {"type", "slider"},
                    {"label", "Target Humidity"},
                    {"unit", "%"},
                    {"min", 30},
                    {"max", 80},
                    {"step", 5},
                    {"read_method", "humidity.get_target"},
                    {"write_method", "humidity.set_target"}
                },
                // Статус обладнання
                {
                    {"id", "dehumidifier"},
                    {"type", "switch"},
                    {"label", "Dehumidifier"},
                    {"read_only", true},
                    {"read_method", "humidity.get_dehumidifier_status"}
                },
                {
                    {"id", "humidifier"},
                    {"type", "switch"},
                    {"label", "Humidifier"},
                    {"read_only", true},
                    {"read_method", "humidity.get_humidifier_status"}
                },
                // Режим роботи
                {
                    {"id", "mode"},
                    {"type", "select"},
                    {"label", "Control Mode"},
                    {"options", {
                        {"auto", "Automatic"},
                        {"manual", "Manual"},
                        {"off", "Off"}
                    }},
                    {"read_method", "humidity.get_mode"},
                    {"write_method", "humidity.set_mode"}
                }
            }},
            // Телеметрія для MQTT
            {"telemetry", {
                {"humidity", {
                    {"source", "state.humidity.current"},
                    {"interval", 30}
                }},
                {"target", {
                    {"source", "state.humidity.target"},
                    {"interval", 300}
                }}
            }},
            // Налаштування алармів
            {"alarms", {
                {"high_humidity", {
                    {"condition", "current_humidity > 85"},
                    {"message", "High humidity alert: {current_humidity}%"},
                    {"severity", "warning"},
                    {"auto_clear", true}
                }},
                {"low_humidity", {
                    {"condition", "current_humidity < 30"},
                    {"message", "Low humidity alert: {current_humidity}%"},
                    {"severity", "warning"},
                    {"auto_clear", true}
                }}
            }}
        };
    }
    
    // Реєстрація RPC методів
    void register_rpc(IJsonRpcRegistrar& rpc) override {
        rpc.register_method("humidity.get_current",
            [this](const json& params, json& result) {
                result["value"] = current_humidity_;
                return ESP_OK;
            });
            
        rpc.register_method("humidity.set_target",
            [this](const json& params, json& result) {
                if (!params.contains("value")) {
                    return ESP_ERR_INVALID_ARG;
                }
                target_humidity_ = params["value"];
                result["success"] = true;
                return ESP_OK;
            });
    }
};
```

## 2. Результат в різних UI

### Web UI (автоматично)
```html
<!-- Згенерована сторінка -->
<div class="module-panel" id="HumidityControl">
    <h2><i class="icon-water-drop"></i> Humidity Control</h2>
    
    <!-- Gauge для поточної вологості -->
    <div class="gauge-container">
        <canvas id="humidity-gauge"></canvas>
        <span class="value">65.3%</span>
    </div>
    
    <!-- Slider для цільової вологості -->
    <div class="control-group">
        <label>Target Humidity</label>
        <input type="range" min="30" max="80" step="5" value="60">
        <span class="value">60%</span>
    </div>
    
    <!-- Статус обладнання -->
    <div class="status-indicators">
        <div class="indicator">
            <span>Dehumidifier</span>
            <div class="led off"></div>
        </div>
        <div class="indicator">
            <span>Humidifier</span>
            <div class="led on"></div>
        </div>
    </div>
    
    <!-- Вибір режиму -->
    <div class="control-group">
        <label>Control Mode</label>
        <select>
            <option value="auto" selected>Automatic</option>
            <option value="manual">Manual</option>
            <option value="off">Off</option>
        </select>
    </div>
</div>
```

### MQTT Topics (автоматично)
```
# Телеметрія
modesp/device123/HumidityControl/humidity → {"value": 65.3, "unit": "%"}
modesp/device123/HumidityControl/target → {"value": 60.0, "unit": "%"}

# Команди
modesp/device123/HumidityControl/set_target ← {"value": 55.0}
modesp/device123/HumidityControl/set_mode ← {"mode": "manual"}

# Home Assistant Discovery
homeassistant/sensor/modesp_humidity/config → {
    "name": "Humidity",
    "device_class": "humidity",
    "unit_of_measurement": "%",
    "state_topic": "modesp/device123/HumidityControl/humidity",
    "value_template": "{{ value_json.value }}"
}
```

### LCD Display (автоматично)
```
┌──────────────────┐
│Humidity Control  │
│Current:    65.3% │
│Target:     60.0% │
│Mode: Automatic   │
└──────────────────┘
```

### Telegram Bot (автоматично)
```
/humiditycontrol - Humidity Control status

Bot response:
📊 Humidity Control
💧 Current: 65.3%
🎯 Target: 60.0%
⚙️ Mode: Automatic
✅ Dehumidifier: OFF
✅ Humidifier: ON

Use /set_humidity <value> to change target
```

### Modbus Registers (автоматично)
```
# Автоматично виділені регістри
40001: Current Humidity (x10, read-only)  // 653 = 65.3%
40002: Target Humidity (x10, read/write)  // 600 = 60.0%
40003: Control Mode (0=off, 1=auto, 2=manual)
40004: Status (bit0=dehumidifier, bit1=humidifier)
```

## 3. Додавання спеціальних функцій

Якщо потрібні специфічні функції для певного UI:

```cpp
// В модулі
nlohmann::json get_ui_schema() const override {
    auto schema = /* базова схема */;
    
    // Додаткові параметри для Web UI
    schema["web_ui"] = {
        {"custom_widget", "humidity-chart"},
        {"refresh_rate", 1000}
    };
    
    // Параметри для LCD
    schema["lcd_ui"] = {
        {"compact_mode", true},
        {"priority_values", {"current_humidity", "target_humidity"}}
    };
    
    return schema;
}
```

## 4. Реєстрація модуля

```cpp
// В main.cpp або application.cpp
void register_modules() {
    // Існуючі модулі...
    
    // Додаємо новий модуль
    auto humidity_module = std::make_unique<HumidityControlModule>();
    ModuleManager::register_module(std::move(humidity_module));
    
    // Всі UI адаптери автоматично отримають повідомлення
    // і створять відповідні інтерфейси
}
```

## Результат

Після додавання модуля **без жодних змін в коді UI адаптерів**:

1. **Web UI** - з'явиться нова сторінка з усіма контролами
2. **MQTT** - створяться топіки для телеметрії та команд
3. **LCD** - додасться нова сторінка в меню
4. **Telegram** - з'явиться команда /humiditycontrol
5. **Modbus** - автоматично виділяться регістри
6. **Будь-який новий UI адаптер** - автоматично підхопить модуль

Це забезпечує:
- ✅ Швидке додавання нового функціоналу
- ✅ Консистентність між різними інтерфейсами
- ✅ Відсутність дублювання коду
- ✅ Легке обслуговування системи