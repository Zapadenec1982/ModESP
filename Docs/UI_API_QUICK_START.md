# UI/API Quick Start Guide

## Adding UI to Your Module - 3 Simple Steps

### 1. Create UI Schema
Create `components/your_module/ui_schema.json`:
```json
{
    "module": "your_module",
    "label": "Your Module",
    "controls": [
        {
            "id": "value1",
            "type": "gauge",
            "label": "Sensor Value",
            "unit": "units",
            "min": 0,
            "max": 100,
            "read_method": "your_module.get_value"
        },
        {
            "id": "setting1", 
            "type": "number",
            "label": "Setting",
            "min": 0,
            "max": 10,
            "read_method": "your_module.get_setting",
            "write_method": "your_module.set_setting"
        }
    ]
}
```

### 2. Implement RPC Methods
In your module cpp file:
```cpp
void YourModule::register_rpc(IJsonRpcRegistrar& rpc) {
    // Read method
    rpc.register_method("your_module.get_value",
        [this](const json& params, json& result) {
            result["value"] = current_value_;
            return ESP_OK;
        });
    
    // Write method  
    rpc.register_method("your_module.set_setting",
        [this](const json& params, json& result) {
            if (params.contains("value")) {
                setting_ = params["value"];
                result["success"] = true;
                return ESP_OK;
            }
            return ESP_ERR_INVALID_ARG;
        });
}
```

### 3. Build and Run
```bash
idf.py build
# UI automatically generated for all protocols!
```

## Result
Your module automatically appears in:
- ✅ Web interface at `http://device-ip/`
- ✅ MQTT topics `modesp/your_module/...`  
- ✅ LCD menu under "Your Module"
- ✅ Modbus registers (if enabled)
- ✅ Any future UI adapters

## Control Types Reference

| Type | Description | Parameters |
|------|-------------|------------|
| `gauge` | Visual meter | min, max, unit, thresholds |
| `value` | Read-only text | unit |
| `number` | Numeric input | min, max, step |
| `switch` | On/off toggle | - |
| `select` | Dropdown | options: {value: label} |
| `slider` | Range control | min, max, step |
| `button` | Action trigger | confirm: true/false |

## Common Patterns

### Temperature Sensor
```json
{
    "id": "temperature",
    "type": "gauge",
    "label": "Temperature", 
    "unit": "°C",
    "min": -40,
    "max": 60,
    "thresholds": {
        "low": 5,
        "normal": [10, 35],
        "high": 40
    },
    "read_method": "sensor.get_temp"
}
```

### On/Off Control
```json
{
    "id": "relay",
    "type": "switch",
    "label": "Relay",
    "read_method": "relay.get_state",
    "write_method": "relay.set_state"
}
```

### Settings Menu
```json
{
    "id": "mode",
    "type": "select",
    "label": "Operating Mode",
    "options": {
        "auto": "Automatic",
        "manual": "Manual", 
        "eco": "Economy"
    },
    "read_method": "system.get_mode",
    "write_method": "system.set_mode"
}
```

## Tips
- Keep control IDs unique within module
- Use descriptive labels (shown to users)
- Include units for numeric values
- Set reasonable min/max limits
- Test with `curl http://device-ip/api/data`

## Need More?
- Full documentation: [UI_API_SYSTEM.md](UI_API_SYSTEM.md)
- Examples: [EXAMPLE_NEW_MODULE_WITH_AUTO_UI.md](EXAMPLE_NEW_MODULE_WITH_AUTO_UI.md)
- Architecture: See technical references