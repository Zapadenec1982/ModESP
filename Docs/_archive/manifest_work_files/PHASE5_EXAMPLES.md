# Phase 5: Component Examples

## 📚 Complete Examples for Adaptive UI Components

### Example 1: Temperature Sensor Manager

```json
{
  "module": {
    "name": "TemperatureSensorManager",
    "version": "2.0.0",
    "type": "MANAGER"
  },
  
  "ui_components": {
    "all_possible": [
      {
        "id": "temp_sensor_type",
        "type": "dropdown",
        "label": "Sensor Type",
        "options": ["None", "DS18B20", "NTC", "DHT22", "BME280"],
        "value_source": "config.temp_sensor.type",
        "applicability": ["always"],
        "access_level": "user",
        "priority": "high",
        "channels": ["lcd", "web", "mqtt"],
        "callbacks": {
          "on_change": "onSensorTypeChanged"
        }
      },
      
      {
        "id": "temp_reading_display",
        "type": "text",
        "label": "Temperature",
        "format": "%.1f°C",
        "value_source": "state.temp_sensor.value",
        "applicability": ["config.temp_sensor.type != 'None'"],
        "access_level": "user",
        "priority": "high",
        "channels": ["lcd", "web", "mqtt", "telegram"],
        "update_rate_ms": 1000
      },
      
      {
        "id": "temp_graph",
        "type": "chart",
        "label": "Temperature History",
        "chart_type": "line",
        "data_source": "state.temp_sensor.history",
        "applicability": [
          "config.temp_sensor.type != 'None'",
          "has_feature('charts')"
        ],
        "access_level": "user",
        "priority": "medium",
        "channels": ["web"],
        "config": {
          "points": 100,
          "update_interval_ms": 5000
        }
      },
      
      {
        "id": "temp_calibrate_btn",
        "type": "button",
        "label": "Calibrate",
        "icon": "tune",
        "action": "temp_sensor.calibrate",
        "applicability": [
          "config.temp_sensor.type != 'None'",
          "has_capability('calibration')"
        ],
        "access_level": "technician",
        "priority": "low",
        "channels": ["lcd", "web"],
        "confirm": true,
        "confirm_message": "Start calibration process?"
      },
      
      {
        "id": "temp_offset_input",
        "type": "number",
        "label": "Offset",
        "min": -10,
        "max": 10,
        "step": 0.1,
        "unit": "°C",
        "value_source": "config.temp_sensor.offset",
        "applicability": [
          "config.temp_sensor.type != 'None'",
          "user_role >= 'technician'"
        ],
        "access_level": "technician",
        "priority": "low",
        "channels": ["web", "mqtt"]
      }
    ]
  },
  
  "driver_registry": {
    "interface": "ITemperatureSensor",
    "path": "temp_sensors",
    "pattern": "*_temp_driver.json"
  }
}
```

### Example 2: DS18B20 Driver with UI Extensions

```json
{
  "driver": {
    "name": "DS18B20TempDriver",
    "version": "1.2.0",
    "interface": "ITemperatureSensor"
  },
  
  "ui_extensions": [
    {
      "id": "ds18b20_resolution",
      "condition": "config.temp_sensor.type == 'DS18B20'",
      "inject_into": "temp_sensor_config",
      "component": {
        "type": "slider",
        "label": "Resolution",
        "min": 9,
        "max": 12,
        "step": 1,
        "value_source": "config.ds18b20.resolution",
        "access_level": "technician",
        "format_label": "%d bits",
        "help_text": "Higher resolution = slower readings"
      }
    },
    
    {
      "id": "ds18b20_parasite_mode",
      "condition": "config.temp_sensor.type == 'DS18B20'",
      "inject_into": "temp_sensor_config",
      "component": {
        "type": "toggle",
        "label": "Parasite Power",
        "value_source": "config.ds18b20.parasite_power",
        "access_level": "technician",
        "help_text": "Enable for 2-wire connection"
      }
    },
    
    {
      "id": "ds18b20_bus_scan",
      "condition": "config.temp_sensor.type == 'DS18B20'",
      "inject_into": "temp_sensor_tools",
      "component": {
        "type": "button",
        "label": "Scan Bus",
        "icon": "search",
        "action": "ds18b20.scan_bus",
        "access_level": "technician",
        "show_result": true
      }
    },
    
    {
      "id": "ds18b20_address_list",
      "condition": [
        "config.temp_sensor.type == 'DS18B20'",
        "state.ds18b20.sensor_count > 0"
      ],
      "inject_into": "temp_sensor_info",
      "component": {
        "type": "list",
        "label": "Found Sensors",
        "value_source": "state.ds18b20.addresses",
        "access_level": "user",
        "item_format": "Sensor %index%: %value%"
      }
    }
  ]
}
```

### Example 3: Complex Conditional UI

```json
{
  "ui_components": {
    "all_possible": [
      {
        "id": "climate_mode",
        "type": "radio_group",
        "label": "Mode",
        "options": ["Off", "Cool", "Heat", "Auto"],
        "value_source": "config.climate.mode",
        "applicability": ["always"],
        "access_level": "user",
        "priority": "high",
        "channels": ["all"]
      },
      
      {
        "id": "climate_setpoint_cool",
        "type": "temperature_input",
        "label": "Cool to",
        "min": 18,
        "max": 30,
        "value_source": "config.climate.setpoint_cool",
        "applicability": [
          "config.climate.mode == 'Cool' OR config.climate.mode == 'Auto'"
        ],
        "access_level": "user",
        "priority": "high",
        "channels": ["all"]
      },
      
      {
        "id": "climate_setpoint_heat",
        "type": "temperature_input",
        "label": "Heat to",
        "min": 10,
        "max": 25,
        "value_source": "config.climate.setpoint_heat",
        "applicability": [
          "config.climate.mode == 'Heat' OR config.climate.mode == 'Auto'"
        ],
        "access_level": "user",
        "priority": "high",
        "channels": ["all"]
      },
      
      {
        "id": "climate_advanced_settings",
        "type": "expandable_section",
        "label": "Advanced Settings",
        "applicability": [
          "config.climate.mode != 'Off'",
          "user_role >= 'technician' OR has_feature('advanced_ui')"
        ],
        "access_level": "user",
        "priority": "low",
        "channels": ["web", "lcd"],
        "contains": [
          "climate_hysteresis",
          "climate_min_cycle_time",
          "climate_compressor_delay"
        ]
      }
    ]
  }
}
```

### Example 4: Multi-Channel Component

```json
{
  "id": "system_status",
  "type": "status_indicator",
  "label": "System Status",
  "value_source": "state.system.status",
  "applicability": ["always"],
  "access_level": "user",
  "priority": "high",
  
  "channel_config": {
    "lcd": {
      "position": "top_right",
      "format": "icon_only",
      "icons": {
        "ok": "✓",
        "warning": "!",
        "error": "✗"
      }
    },
    
    "web": {
      "format": "badge",
      "colors": {
        "ok": "#4CAF50",
        "warning": "#FF9800",
        "error": "#F44336"
      },
      "show_details_on_hover": true
    },
    
    "mqtt": {
      "topic": "status/system",
      "format": "json",
      "include_timestamp": true
    },
    
    "telegram": {
      "format": "emoji_text",
      "emojis": {
        "ok": "✅",
        "warning": "⚠️",
        "error": "🚨"
      },
      "include_in_summary": true
    }
  }
}
```

### Example 5: Dynamic List Component

```json
{
  "id": "alarm_list",
  "type": "dynamic_list",
  "label": "Active Alarms",
  "data_source": "state.alarms.active",
  "applicability": ["state.alarms.count > 0"],
  "access_level": "user",
  "priority": "high",
  "channels": ["all"],
  
  "list_config": {
    "max_items": 10,
    "sort_by": "severity",
    "sort_order": "desc",
    
    "item_template": {
      "lcd": "{severity_icon} {name}",
      "web": "<div class='alarm-{severity}'>{icon} {name} - {description}</div>",
      "mqtt": {
        "code": "{code}",
        "name": "{name}",
        "severity": "{severity}",
        "timestamp": "{timestamp}"
      }
    },
    
    "empty_message": {
      "lcd": "No alarms",
      "web": "✓ System operating normally",
      "mqtt": {"status": "ok", "count": 0}
    }
  },
  
  "actions": {
    "acknowledge": {
      "access_level": "technician",
      "channels": ["lcd", "web"]
    },
    "silence": {
      "access_level": "user",
      "channels": ["all"]
    }
  }
}
```

### Example 6: Role-Based Component Visibility

```json
{
  "ui_components": {
    "all_possible": [
      {
        "id": "basic_info",
        "type": "info_panel",
        "applicability": ["always"],
        "access_level": "viewer",
        "content": {
          "Temperature": "{state.temp}°C",
          "Status": "{state.status}"
        }
      },
      
      {
        "id": "control_panel",
        "type": "control_group",
        "applicability": ["user_role >= 'operator'"],
        "access_level": "operator",
        "controls": ["start_btn", "stop_btn", "mode_select"]
      },
      
      {
        "id": "config_panel",
        "type": "settings_group",
        "applicability": ["user_role >= 'technician'"],
        "access_level": "technician",
        "settings": ["setpoints", "timers", "alarms"]
      },
      
      {
        "id": "service_panel",
        "type": "diagnostic_group",
        "applicability": ["user_role >= 'service'"],
        "access_level": "service",
        "tools": ["sensor_test", "actuator_test", "comm_test"]
      },
      
      {
        "id": "admin_panel",
        "type": "admin_group",
        "applicability": ["user_role == 'admin'"],
        "access_level": "admin",
        "features": ["user_management", "system_config", "factory_reset"]
      }
    ]
  }
}
```

## 🎯 Best Practices from Examples

1. **Use Clear IDs**: Make component IDs descriptive and unique
2. **Simple Conditions**: Keep applicability conditions readable
3. **Appropriate Priority**: Set priority based on actual usage
4. **Channel-Specific Config**: Optimize for each channel's capabilities
5. **Consistent Access Levels**: Match functionality with user roles
6. **Helpful Metadata**: Include help_text, icons, and formatting

## 📚 More Resources

- [Component Type Reference](COMPONENT_TYPES.md)
- [Condition Syntax Guide](CONDITION_SYNTAX.md)
- [Channel Configuration](CHANNEL_CONFIG.md)
- [Access Level Guide](ACCESS_LEVELS.md)