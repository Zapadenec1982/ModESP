# Module Manifest Specification v1.0

## Структура маніфесту

```json
{
  "module": {
    "name": "string",          // Ім'я класу модуля
    "version": "string",       // Семантична версія
    "description": "string",   // Опис модуля
    "type": "enum",           // CORE, STANDARD, OPTIONAL
    "priority": "enum",       // CRITICAL, HIGH, NORMAL, LOW
    "dependencies": ["string"], // Залежності
    "resources": {
      "stack_size": "number",
      "heap_usage": "string",
      "update_budget_ms": "number"
    }
  },
  
  "configuration": {
    "config_file": "string",   // Ім'я JSON файлу конфігурації
    "schema": {}              // JSON Schema
  },
  
  "apis": {
    "static": [],             // Статичні API
    "dynamic": []             // Умовні API
  }  
  "ui": {
    "static": {},             // Базовий UI
    "dynamic": {}             // Умовні UI елементи
  },
  
  "shared_state": {
    "publishes": {},          // SharedState keys
    "subscribes": {}          // SharedState subscriptions
  },
  
  "event_bus": {
    "publishes": {},          // Events
    "subscribes": {}          // Event handlers
  }
}
```

## API Definition

```json
{
  "method": "module.action",
  "handler": "cpp_function_name",
  "description": "string",
  "access_level": "user|technician|admin|supervisor",
  "params": {
    "param_name": {
      "type": "string|number|boolean|object|array",
      "required": true|false,
      "validation": {}
    }
  }  "returns": {
    "type": "object",
    "properties": {}
  }
}
```

## UI Definition

### Static UI
```json
{
  "menu": {
    "label": "string",
    "icon": "string",
    "order": "number"
  },
  "pages": [
    {
      "id": "string",
      "label": "string",
      "layout": "grid|form|custom",
      "access_level": "string",
      "widgets": []
    }
  ]
}
```

### Dynamic UI
```json
{
  "conditions": [    {
      "when": "expression",
      "add_to_page": "page_id",
      "section": {
        "label": "string",
        "fields": []
      }
    }
  ]
}
```

## Condition Expressions

Підтримувані вирази:
- `config.path.to.value == 'expected'`
- `has_feature('feature_name')`
- `has_sensor_type('type')`
- `module_enabled('module_name')`

## Driver Manifest Structure

Driver manifests відрізняються від module manifests:

```json
{
  "driver": {
    "name": "string",
    "type": "string",        // temperature, humidity, gpio, etc
    "version": "string",
    "hal_requirements": ["string"]  // Required HAL modules
  },
  
  "capabilities": {
    // Driver-specific capabilities
  },
  
  "configuration": {
    "schema": {}  // Driver configuration
  },
  
  "apis": {
    "specific": []  // Driver-specific methods
  },
  
  "ui_extensions": {
    "config_fields": []  // UI fields to inject
  }
}
```

## Manager Registry

Managers що агрегують драйвери мають додаткову секцію:

```json
{
  "driver_registry": {
    "path": "string",           // Path to drivers
    "pattern": "string",        // Manifest file pattern
    "interface": "string"       // C++ interface name
  }
}
```