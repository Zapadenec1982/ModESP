# ModESP UI/API System Documentation

## Table of Contents
1. [Overview](#overview)
2. [Architecture](#architecture)
3. [Implementation](#implementation)
4. [Module Integration](#module-integration)
5. [UI Adapters](#ui-adapters)
6. [API Reference](#api-reference)
7. [Examples](#examples)

---

## 1. Overview

### Purpose
The ModESP UI/API system provides a unified, extensible interface for controlling and monitoring the system through multiple protocols (Web, MQTT, LCD, Modbus, etc.) with automatic UI generation from module metadata.

### Key Features
- **Automatic UI Generation**: Modules describe their UI once, get interfaces for all protocols
- **Compile-time Optimization**: UI structure generated during build, minimal runtime overhead
- **Protocol Independence**: Each UI adapter works independently
- **Zero-configuration**: New modules automatically appear in all UIs

### Design Principles
1. **Decentralized**: No monolithic API gateway
2. **Resource-efficient**: Optimized for ESP32 constraints
3. **Extensible**: Easy to add new protocols/UIs
4. **Type-safe**: Contract-based communication

---

## 2. Architecture

### System Overview
```
┌─────────────────────────────────────────────────┐
│              Business Logic Modules              │
│         (with UI schemas and RPC methods)        │
└─────────────┬──────────────────────┬────────────┘
              │                      │
              ▼                      ▼
      ┌───────────────┐      ┌──────────────┐
      │ Compile-time  │      │   Runtime    │
      │  Generation   │      │  Components  │
      └───────┬───────┘      └──────┬───────┘
              │                      │
    ┌─────────┴──────────────────────┴─────────┐
    │          Generated Headers               │
    │  (web_ui.h, mqtt_topics.h, lcd_menu.h)  │
    └─────────┬──────────────────────┬─────────┘
              │                      │
    ┌─────────┴─────────┐  ┌────────┴────────┐
    │   UI Adapters     │  │  Communication  │
    │ (Web, MQTT, LCD)  │  │ (EventBus, RPC) │
    └───────────────────┘  └─────────────────┘
```

### Core Components

#### 1. Module UI Schema
Each module provides a `ui_schema.json` file describing its UI elements, telemetry, and alarms.

#### 2. Compile-time Generator
Python script that processes schemas and generates optimized C++ headers with UI structures in PROGMEM.

#### 3. UI Adapters
Independent modules that interpret UI schemas for their specific protocol (Web, MQTT, LCD, etc.).

#### 4. Communication Layer
- **EventBus**: Asynchronous module communication
- **SharedState**: Centralized data storage
- **RPC Interface**: Direct method calls

### Resource Usage

| Component | RAM Usage | Flash Usage | CPU Load |
|-----------|-----------|-------------|----------|
| Web UI | 3-7 KB | 25-40 KB | Low |
| MQTT | 2-5 KB | 10-15 KB | Minimal |
| LCD | 1-3 KB | 5-10 KB | Minimal |
| Core | 5-10 KB | 15-20 KB | Low |

---

## 3. Implementation

### Build Process
```bash
1. Developer creates/modifies ui_schema.json
2. Build system runs ui_generator.py
3. Generator creates optimized headers
4. C++ compiler includes generated headers
5. Final binary contains pre-built UI structures
```

### File Structure
```
ModESP/
├── components/
│   ├── [module_name]/
│   │   ├── ui_schema.json      # Module UI description
│   │   └── [module].cpp         # Business logic + RPC
│   ├── ui/                      # Web UI adapter
│   ├── mqtt_ui/                 # MQTT adapter
│   └── lcd_ui/                  # LCD adapter
├── tools/
│   └── ui_generator.py          # Build-time generator
├── main/generated/              # Generated headers
│   ├── web_ui_generated.h
│   ├── mqtt_topics_generated.h
│   └── lcd_menu_generated.h
└── cmake/
    └── ui_generation.cmake      # Build integration
```

### Key Interfaces

#### BaseModule Extension
```cpp
class BaseModule {
    // Standard module interface...
    
    // UI-specific additions
    virtual nlohmann::json get_ui_schema() const;
    virtual nlohmann::json get_capabilities() const;
    virtual void register_rpc(IJsonRpcRegistrar& rpc);
};
```

#### IJsonRpcRegistrar
```cpp
class IJsonRpcRegistrar {
    virtual esp_err_t register_method(
        const std::string& method,
        JsonRpcHandler handler,
        const std::string& description = ""
    ) = 0;
};
```

---

## 4. Module Integration

### UI Schema Format
```json
{
    "module": "module_name",
    "label": "Display Name",
    "version": "1.0",
    "controls": [
        {
            "id": "temperature",
            "type": "gauge",
            "label": "Temperature",
            "unit": "°C",
            "min": -40,
            "max": 60,
            "read_method": "module.get_temperature",
            "write_method": "module.set_temperature"
        }
    ],
    "telemetry": {
        "temperature": {
            "source": "state.module.temperature",
            "interval": 60
        }
    },
    "alarms": {
        "high_temp": {
            "condition": "temperature > 40",
            "message": "High temperature: {temperature}°C",
            "severity": "warning"
        }
    }
}
```

### Control Types
- **gauge**: Visual meter with min/max
- **value**: Read-only display
- **number**: Numeric input
- **switch**: Boolean toggle
- **select**: Dropdown selection
- **slider**: Range input
- **button**: Action trigger
- **chart**: Time-series graph

### RPC Implementation
```cpp
void MyModule::register_rpc(IJsonRpcRegistrar& rpc) {
    rpc.register_method("module.get_temperature",
        [this](const json& params, json& result) {
            result["value"] = current_temperature_;
            return ESP_OK;
        });
        
    rpc.register_method("module.set_temperature",
        [this](const json& params, json& result) {
            if (!params.contains("value")) {
                return ESP_ERR_INVALID_ARG;
            }
            target_temperature_ = params["value"];
            result["success"] = true;
            return ESP_OK;
        });
}
```

---

## 5. UI Adapters

### Web UI Adapter
- Serves pre-generated HTML/JS/CSS from PROGMEM
- REST API endpoints for data/commands
- WebSocket for real-time updates
- Automatic responsive layout

### MQTT UI Adapter
- Auto-generated topics from schema
- Home Assistant discovery support
- Configurable QoS and retention
- Telemetry publishing on intervals

### LCD UI Adapter
- Auto-generated menu structure
- Button navigation
- Compact data display
- Configurable refresh rates

### Adding New Adapters
1. Create class inheriting from `UIAdapterBase`
2. Implement `register_ui_handlers()`
3. Process module schemas for your protocol
4. Register in module manager

---

## 6. API Reference

### System Contracts
All event names and state keys are defined in `system_contract.h`:

```cpp
namespace ModespContract {
    namespace State {
        constexpr auto SensorTemperature = "sensor.temperature";
        constexpr auto ActuatorCompressor = "actuator.compressor";
        // ...
    }
    namespace Event {
        constexpr auto SensorReading = "sensor.reading";
        constexpr auto ApiRequest = "api.request";
        // ...
    }
}
```

### REST API Endpoints
- `GET /` - Main UI page
- `GET /api/data` - All current values
- `POST /api/rpc` - Execute RPC method
- `GET /api/schema` - Get UI schemas

### MQTT Topics
- `modesp/[module]/[telemetry]` - Telemetry data
- `modesp/[module]/[control]/set` - Commands
- `modesp/status` - System status
- `homeassistant/[component]/modesp_[id]/config` - Discovery

### WebSocket Events
- `connect` - Client connected
- `update` - Data update
- `error` - Error notification
- `alarm` - Alarm triggered

---

## 7. Examples

### Adding a New Module
1. Create module with UI schema:
```cpp
// humidity_control.h
class HumidityControl : public BaseModule {
    nlohmann::json get_ui_schema() const override {
        return load_schema("humidity_control/ui_schema.json");
    }
    
    void register_rpc(IJsonRpcRegistrar& rpc) override {
        // Register methods...
    }
};
```

2. Create `ui_schema.json`
3. Build project - UI automatically generated
4. Module appears in all active UI adapters

### Custom UI Behavior
```json
{
    "controls": [...],
    "web_ui": {
        "custom_widget": "humidity-chart",
        "refresh_rate": 1000
    },
    "lcd_ui": {
        "compact_mode": true,
        "priority_values": ["current", "target"]
    }
}
```

### Runtime Configuration
```cpp
// Enable/disable UI elements dynamically
if (config.hasFeatureX) {
    ui_adapter->enable_control("module.feature_x");
}
```

---

## Best Practices

1. **Keep schemas simple** - Complex UIs slow down all adapters
2. **Use standard control types** - Custom widgets require adapter changes  
3. **Minimize telemetry frequency** - Respect bandwidth/power constraints
4. **Version your schemas** - For backward compatibility
5. **Test on all adapters** - Ensure consistent behavior

## Troubleshooting

| Issue | Solution |
|-------|----------|
| UI not updating | Check RPC method implementation |
| Missing in adapter | Verify ui_schema.json syntax |
| High memory usage | Reduce schema complexity |
| Build errors | Check Python path and permissions |

---

*This documentation consolidates the ModESP UI/API system design. For implementation details, see the source code and examples.*