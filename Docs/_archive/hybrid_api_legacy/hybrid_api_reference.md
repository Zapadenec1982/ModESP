# Hybrid API Reference

> **Status**: ✅ Complete  
> **Version**: 1.0  
> **Base URL**: `http://{device_ip}/api/jsonrpc`

## 🎯 Overview

Complete API reference for the Hybrid API Contract system. All APIs follow JSON-RPC 2.0 specification with additional REST endpoint support.

## 📋 API Categories

### **🔧 Static APIs (Always Available)**
- [System APIs](#system-apis) - System status and control
- [Sensor Base APIs](#sensor-base-apis) - Generic sensor operations  
- [Actuator Base APIs](#actuator-base-apis) - Generic actuator operations
- [Climate APIs](#climate-apis) - Climate control operations
- [Network APIs](#network-apis) - Connectivity and networking
- [Configuration APIs](#configuration-apis) - System configuration

### **⚡ Dynamic APIs (Configuration-Dependent)**
- [Sensor-Specific APIs](#sensor-specific-apis) - Driver-dependent operations
- [Defrost APIs](#defrost-apis) - Defrost strategy operations
- [Scenario APIs](#scenario-apis) - Custom scenario operations

### **⚙️ Configuration Management APIs**
- [Configuration Update APIs](#configuration-update-apis) - Configuration changes
- [Restart Management APIs](#restart-management-apis) - System restart control

---

## 🔧 Static APIs

### **System APIs**

#### `system.get_status`
Get comprehensive system status information.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "system.get_status",
    "id": 1
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "uptime_seconds": 3600,
        "free_heap": 245760,
        "min_free_heap": 200000,
        "reset_reason": 1,
        "chip_model": 1,
        "timestamp": 1704441600000000
    },
    "id": 1
}
```

**Response Fields:**
- `uptime_seconds` (integer): System uptime in seconds
- `free_heap` (integer): Current free heap in bytes
- `min_free_heap` (integer): Minimum free heap since boot
- `reset_reason` (integer): ESP32 reset reason code
- `chip_model` (integer): ESP32 chip model identifier
- `timestamp` (integer): Current timestamp in microseconds

---

#### `system.get_uptime`
Get system uptime in various formats.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "system.get_uptime",
    "id": 2
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "uptime_seconds": 3600,
        "uptime_milliseconds": 3600000,
        "uptime_microseconds": 3600000000
    },
    "id": 2
}
```

---

#### `system.restart`
Restart the system with optional delay.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "system.restart",
    "params": {
        "delay_seconds": 5
    },
    "id": 3
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "message": "System restart initiated",
        "delay_seconds": 5
    },
    "id": 3
}
```

---

#### `system.get_memory_info`
Get detailed memory usage information.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "system.get_memory_info",
    "id": 4
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "free_heap": 245760,
        "minimum_free_heap": 200000,
        "largest_free_block": 180000
    },
    "id": 4
}
```

---

### **Sensor Base APIs**

#### `sensor.get_all_readings`
Get readings from all configured sensors.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.get_all_readings",
    "id": 5
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "temperature": {
            "value": 23.5,
            "unit": "celsius"
        },
        "humidity": {
            "value": 45.2,
            "unit": "percent"
        },
        "pressure": {
            "value": 1.2,
            "unit": "bar"
        },
        "door_open": false,
        "timestamp": 1704441600000000
    },
    "id": 5
}
```

---

#### `sensor.get_temperature`
Get current temperature reading.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.get_temperature",
    "id": 6
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "value": 23.5,
        "unit": "celsius",
        "timestamp": 1704441600000000
    },
    "id": 6
}
```

---

#### `sensor.get_humidity`
Get current humidity reading.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.get_humidity",
    "id": 7
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "value": 45.2,
        "unit": "percent",
        "timestamp": 1704441600000000
    },
    "id": 7
}
```

---

### **Actuator Base APIs**

#### `actuator.get_compressor_status`
Get compressor status and runtime information.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "actuator.get_compressor_status",
    "id": 8
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "state": true,
        "runtime": 1800,
        "cycles": 5,
        "timestamp": 1704441600000000
    },
    "id": 8
}
```

**Response Fields:**
- `state` (boolean): Current compressor state (on/off)
- `runtime` (integer): Total runtime in seconds
- `cycles` (integer): Number of on/off cycles
- `timestamp` (integer): Last update timestamp

---

#### `actuator.get_all_states`
Get status of all actuators.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "actuator.get_all_states",
    "id": 9
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "compressor": {
            "state": true,
            "runtime": 1800
        },
        "evaporator_fan": {
            "state": true,
            "speed": 75
        },
        "defrost_heater": {
            "state": false,
            "power": 0.0
        },
        "light": true,
        "timestamp": 1704441600000000
    },
    "id": 9
}
```

---

### **Climate APIs**

#### `climate.get_setpoint`
Get current temperature setpoint.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "climate.get_setpoint",
    "id": 10
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "setpoint": 20.0,
        "unit": "celsius",
        "timestamp": 1704441600000000
    },
    "id": 10
}
```

---

#### `climate.set_setpoint`
Set temperature setpoint.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "climate.set_setpoint",
    "params": {
        "value": 18.5
    },
    "id": 11
}
```

**Parameters:**
- `value` (number, required): New setpoint in Celsius (-40.0 to 60.0)

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "success": true,
        "setpoint": 18.5
    },
    "id": 11
}
```

**Errors:**
- `-32602`: Invalid params (missing value or out of range)

---

#### `climate.get_mode`
Get current climate control mode.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "climate.get_mode",
    "id": 12
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "mode": "cooling",
        "timestamp": 1704441600000000
    },
    "id": 12
}
```

**Modes:**
- `cooling`: Cooling mode active
- `heating`: Heating mode active  
- `idle`: No active control

---

### **Network APIs**

#### `wifi.get_status`
Get WiFi connection status.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "wifi.get_status",
    "id": 13
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "connected": true,
        "ssid": "MyNetwork",
        "rssi": -45,
        "timestamp": 1704441600000000
    },
    "id": 13
}
```

**Response Fields:**
- `connected` (boolean): Connection status
- `ssid` (string): Connected network name
- `rssi` (integer): Signal strength in dBm
- `timestamp` (integer): Status timestamp

---

#### `network.get_ip_address`
Get current IP address.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "network.get_ip_address",
    "id": 14
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "ip_address": "192.168.1.100",
        "timestamp": 1704441600000000
    },
    "id": 14
}
```

---

#### `mqtt.get_status`
Get MQTT connection status.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "mqtt.get_status",
    "id": 15
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "connected": true,
        "timestamp": 1704441600000000
    },
    "id": 15
}
```

---

### **Configuration APIs**

#### `config.get_sensors`
Get current sensor configuration.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "config.get_sensors",
    "id": 16
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "sensors": [
            {
                "role": "temperature_1",
                "type": "DS18B20_Async",
                "config": {
                    "resolution": 12,
                    "use_crc": true
                }
            }
        ],
        "timestamp": 1704441600000000
    },
    "id": 16
}
```

---

#### `config.get_available_sensor_types`
Get list of available sensor driver types.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "config.get_available_sensor_types",
    "id": 17
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "available_types": [
            "DS18B20_Async",
            "NTC",
            "GPIO_Input"
        ],
        "timestamp": 1704441600000000
    },
    "id": 17
}
```

---

#### `config.get_system`
Get system configuration and status.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "config.get_system",
    "id": 18
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "firmware_version": "1.0.0",
        "compilation_date": "Jan  5 2025 12:00:00",
        "free_heap": 245760,
        "uptime_seconds": 3600
    },
    "id": 18
}
```

---

## ⚡ Dynamic APIs

### **Sensor-Specific APIs**

Dynamic APIs are generated based on configured sensors and their driver schemas.

#### `sensor.{role}.get_value`
Get value from specific sensor by role.

**Example: DS18B20 Temperature Sensor**

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.temperature_1.get_value",
    "id": 19
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "sensor_role": "temperature_1",
        "value": 23.5,
        "unit": "celsius",
        "timestamp": 1704441600000000
    },
    "id": 19
}
```

---

#### `sensor.{role}.get_diagnostics`
Get diagnostics for specific sensor.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.temperature_1.get_diagnostics",
    "id": 20
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "sensor_role": "temperature_1",
        "status": "OK",
        "last_reading_time": 1704441600000000,
        "error_count": 0,
        "response_time_ms": 25
    },
    "id": 20
}
```

---

#### `sensor.{role}.calibrate`
Calibrate specific sensor.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.temperature_1.calibrate",
    "params": {
        "reference_value": 25.0
    },
    "id": 21
}
```

**Parameters:**
- `reference_value` (number, required): Known reference temperature

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "sensor_role": "temperature_1",
        "calibration_success": true,
        "reference_value": 25.0,
        "old_offset": 0.0,
        "new_offset": 0.5
    },
    "id": 21
}
```

---

#### DS18B20-Specific APIs

When a DS18B20_Async sensor is configured, these additional APIs become available:

**`sensor.{role}.set_resolution`**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.temperature_1.set_resolution",
    "params": {
        "value": 12
    },
    "id": 22
}
```

**Valid values:** 9, 10, 11, 12 (bits)

**`sensor.{role}.set_use_crc`**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.temperature_1.set_use_crc",
    "params": {
        "value": true
    },
    "id": 23
}
```

---

#### NTC-Specific APIs

When an NTC sensor is configured, these additional APIs become available:

**`sensor.{role}.set_ntc_type`**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.temperature_2.set_ntc_type",
    "params": {
        "value": "10K_3950"
    },
    "id": 24
}
```

**Valid values:** "10K_3950", "10K_3435", "100K_3950"

**`sensor.{role}.set_series_resistor`**
```json
{
    "jsonrpc": "2.0",
    "method": "sensor.temperature_2.set_series_resistor",
    "params": {
        "value": 10000
    },
    "id": 25
}
```

**Valid range:** 1000 - 100000 ohms

---

### **Defrost APIs**

Generated based on configured defrost strategy.

#### Time-Based Defrost APIs

**`defrost.set_interval`**
```json
{
    "jsonrpc": "2.0",
    "method": "defrost.set_interval",
    "params": {
        "hours": 6
    },
    "id": 26
}
```

**`defrost.set_duration`**
```json
{
    "jsonrpc": "2.0",
    "method": "defrost.set_duration",
    "params": {
        "minutes": 30
    },
    "id": 27
}
```

---

#### Temperature-Based Defrost APIs

**`defrost.set_trigger_temp`**
```json
{
    "jsonrpc": "2.0",
    "method": "defrost.set_trigger_temp",
    "params": {
        "temperature": -5.0
    },
    "id": 28
}
```

**`defrost.set_end_temp`**
```json
{
    "jsonrpc": "2.0",
    "method": "defrost.set_end_temp",
    "params": {
        "temperature": 8.0
    },
    "id": 29
}
```

---

#### Adaptive Defrost APIs

**`defrost.set_algorithm_params`**
```json
{
    "jsonrpc": "2.0",
    "method": "defrost.set_algorithm_params",
    "params": {
        "learning_period_hours": 168,
        "efficiency_threshold": 0.85
    },
    "id": 30
}
```

**`defrost.get_prediction`**
```json
{
    "jsonrpc": "2.0",
    "method": "defrost.get_prediction",
    "id": 31
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "next_defrost_in_minutes": 45,
        "confidence": 0.85
    },
    "id": 31
}
```

---

## ⚙️ Configuration Management APIs

### **Configuration Update APIs**

#### `config.update_sensors`
Update sensor configuration (requires restart).

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "config.update_sensors",
    "params": {
        "sensors": [
            {
                "role": "temperature_1",
                "type": "NTC",
                "config": {
                    "ntc_type": "10K_3950",
                    "series_resistor": 10000
                }
            }
        ]
    },
    "id": 32
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "success": true,
        "restart_required": true,
        "message": "Configuration saved. System restart required to apply changes."
    },
    "id": 32
}
```

**Errors:**
- `-32602`: Invalid configuration format
- `-32603`: Validation failed

---

#### `config.update_defrost`
Update defrost configuration (requires restart).

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "config.update_defrost",
    "params": {
        "type": "ADAPTIVE",
        "config": {
            "learning_period_hours": 168,
            "efficiency_threshold": 0.85
        }
    },
    "id": 33
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "success": true,
        "restart_required": true,
        "message": "Defrost configuration saved. Restart required."
    },
    "id": 33
}
```

---

### **Restart Management APIs**

#### `system.restart_for_config`
Restart system to apply configuration changes.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "system.restart_for_config",
    "id": 34
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "message": "System restart scheduled",
        "reason": "sensor_configuration_changed"
    },
    "id": 34
}
```

**Errors:**
- `-32603`: No restart required

---

#### `config.get_restart_status`
Check if restart is required for configuration changes.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "config.get_restart_status",
    "id": 35
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "restart_required": true,
        "restart_reason": "sensor_configuration_changed"
    },
    "id": 35
}
```

---

#### `system.get_api_documentation`
Get complete API documentation with categorization.

**Request:**
```json
{
    "jsonrpc": "2.0",
    "method": "system.get_api_documentation",
    "id": 36
}
```

**Response:**
```json
{
    "jsonrpc": "2.0",
    "result": {
        "static_methods": [
            {
                "method": "system.get_status",
                "description": "System status information"
            }
        ],
        "dynamic_methods": [
            {
                "method": "sensor.temperature_1.get_value",
                "description": "Get current value from sensor temperature_1"
            }
        ],
        "total_methods": 25,
        "hybrid_initialized": true
    },
    "id": 36
}
```

---

## 🔗 REST Endpoint Support

All JSON-RPC methods are also available as REST endpoints:

### **GET Endpoints**
```bash
# System status
GET /api/status
# Equivalent to: {"method": "system.get_status"}

# Sensor readings  
GET /api/sensors
# Equivalent to: {"method": "sensor.get_all_readings"}

# Climate setpoint
GET /api/climate/setpoint
# Equivalent to: {"method": "climate.get_setpoint"}
```

### **POST Endpoints**
```bash
# Set climate setpoint
POST /api/climate/setpoint
Content-Type: application/json
{"value": 18.5}

# Update configuration
POST /api/config/sensors
Content-Type: application/json
{"sensors": [...]}
```

---

## 🛡️ Error Handling

### **Standard JSON-RPC Errors**
- `-32700`: Parse error (Invalid JSON)
- `-32600`: Invalid Request  
- `-32601`: Method not found
- `-32602`: Invalid params
- `-32603`: Internal error

### **Application-Specific Errors**
- `1001`: Validation failed
- `1002`: Module not ready
- `2001`: Sensor timeout
- `3001`: Actuator fault

### **Error Response Format**
```json
{
    "jsonrpc": "2.0",
    "error": {
        "code": -32602,
        "message": "Invalid params",
        "data": {
            "field": "value",
            "error": "Value out of range"
        }
    },
    "id": 1
}
```

---

## 📊 Response Time Expectations

| API Category | Expected Response Time |
|--------------|----------------------|
| System APIs | 10-50ms |
| Sensor Base APIs | 30-80ms |
| Actuator APIs | 30-60ms |
| Dynamic Sensor APIs | 50-100ms |
| Configuration APIs | 100-300ms |
| Restart APIs | 50-200ms |

---

## 🧪 Testing Examples

### **cURL Examples**
```bash
# Get system status
curl -X POST http://192.168.1.100/api/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"system.get_status","id":1}'

# Set climate setpoint
curl -X POST http://192.168.1.100/api/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"climate.set_setpoint","params":{"value":18.5},"id":2}'

# Update sensor configuration
curl -X POST http://192.168.1.100/api/jsonrpc \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","method":"config.update_sensors","params":{"sensors":[{"role":"temp1","type":"NTC"}]},"id":3}'
```

### **JavaScript Examples**
```javascript
// Fetch system status
async function getSystemStatus() {
    const response = await fetch('/api/jsonrpc', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            jsonrpc: '2.0',
            method: 'system.get_status',
            id: 1
        })
    });
    
    const data = await response.json();
    return data.result;
}

// Set climate setpoint
async function setSetpoint(temperature) {
    const response = await fetch('/api/jsonrpc', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            jsonrpc: '2.0',
            method: 'climate.set_setpoint',
            params: {value: temperature},
            id: 2
        })
    });
    
    const data = await response.json();
    if (data.error) {
        throw new Error(data.error.message);
    }
    return data.result;
}
```

### **Python Examples**
```python
import requests
import json

class ModESPAPI:
    def __init__(self, base_url):
        self.base_url = base_url
        self.session = requests.Session()
        self.id_counter = 1
    
    def call(self, method, params=None):
        payload = {
            'jsonrpc': '2.0',
            'method': method,
            'id': self.id_counter
        }
        if params:
            payload['params'] = params
        
        response = self.session.post(
            f'{self.base_url}/api/jsonrpc',
            json=payload,
            timeout=30
        )
        
        data = response.json()
        if 'error' in data:
            raise Exception(f"API Error: {data['error']['message']}")
        
        self.id_counter += 1
        return data['result']
    
    def get_system_status(self):
        return self.call('system.get_status')
    
    def set_climate_setpoint(self, temperature):
        return self.call('climate.set_setpoint', {'value': temperature})
    
    def update_sensor_config(self, sensors):
        return self.call('config.update_sensors', {'sensors': sensors})

# Usage
api = ModESPAPI('http://192.168.1.100')
status = api.get_system_status()
print(f"System uptime: {status['uptime_seconds']} seconds")
```

---

The Hybrid API system provides a comprehensive, type-safe, and well-documented interface for all system operations. The combination of static and dynamic APIs ensures both reliability and flexibility for industrial refrigeration applications.
