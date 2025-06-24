# Оптимізована архітектура UI: Compile-time + Runtime генерація

## Проблема
- ESP32 має обмежені ресурси (RAM, CPU)
- Постійна генерація UI з JSON схем - дорога операція
- Статичні елементи UI не змінюються після компіляції
- Зберігання HTML/JS в runtime займає багато RAM

## Рішення: Двоетапна генерація

### 1. Compile-time генерація (Python скрипт)

```
┌─────────────────────────────────────────┐
│         ui_schema.json файли            │
│    (від кожного модуля)                 │
└─────────────────┬───────────────────────┘
                  │
                  ▼
        ┌─────────────────────┐
        │  ui_generator.py     │
        │  (Pre-build script)  │
        └─────────┬───────────┘
                  │
    ┌─────────────┼─────────────────────┐
    ▼             ▼                     ▼
generated/    generated/           generated/
web_ui.h      mqtt_topics.h       modbus_map.h
(HTML/JS)     (Topic defines)     (Register map)
    │             │                     │
    └─────────────┴─────────────────────┘
                  │
                  ▼
            Компіляція ESP32
```

### 2. Структура згенерованих файлів

#### generated/web_ui.h
```cpp
// Автоматично згенеровано з UI схем
#pragma once

// Мінімізований HTML як PROGMEM строки
const char INDEX_HTML[] PROGMEM = R"(
<!DOCTYPE html><html><head>...</head>
<body>
<div id="app">
  <!-- Статична структура всіх модулів -->
  <div class="module" data-module="sensor">
    <h2>Sensors</h2>
    <div data-bind="sensor.temperature" class="gauge"></div>
    <input data-bind="sensor.threshold" type="number">
  </div>
  <!-- Інші модулі... -->
</div>
</body></html>
)";

// JavaScript теж в PROGMEM
const char APP_JS[] PROGMEM = R"(
// Мінімізований JS код
const modules={sensor:{temperature:{type:"gauge",min:-40,max:60}}}
)";

// CSS
const char STYLE_CSS[] PROGMEM = R"(
/* Мінімізовані стилі */
)";

// Метадані для runtime
struct UIElement {
    const char* id;
    const char* rpc_method;
    uint8_t type;  // enum замість string
};

// Таблиця елементів (в PROGMEM)
const UIElement UI_ELEMENTS[] PROGMEM = {
    {"sensor_temp", "sensor.get_temperature", UI_TYPE_GAUGE},
    {"sensor_threshold", "sensor.get_threshold", UI_TYPE_NUMBER},
    // ...
};
const size_t UI_ELEMENTS_COUNT = sizeof(UI_ELEMENTS) / sizeof(UIElement);
```

#### generated/mqtt_topics.h
```cpp
// MQTT топіки як константи
#pragma once

// Телеметрія
#define MQTT_TOPIC_SENSOR_TEMP "modesp/sensor/temperature"
#define MQTT_TOPIC_SENSOR_HUMIDITY "modesp/sensor/humidity"

// Команди  
#define MQTT_TOPIC_CMD_SETPOINT "modesp/climate/setpoint/set"

// Мапінг для runtime
struct MqttTopicMap {
    const char* topic;
    const char* state_key;
    uint16_t interval_s;
};

const MqttTopicMap MQTT_TELEMETRY[] PROGMEM = {
    {MQTT_TOPIC_SENSOR_TEMP, "sensor.temperature", 60},
    {MQTT_TOPIC_SENSOR_HUMIDITY, "sensor.humidity", 60},
    // ...
};
```

#### generated/lcd_menu.h
```cpp
// Структура меню для LCD
#pragma once

struct LcdMenuItem {
    const char* label;
    uint8_t type;  // MENU_VALUE, MENU_SUBMENU, MENU_ACTION
    union {
        const char* state_key;  // для значень
        uint8_t submenu_id;     // для підменю
        uint8_t action_id;      // для дій
    };
};

const LcdMenuItem MAIN_MENU[] PROGMEM = {
    {"Temperature", MENU_VALUE, {.state_key = "sensor.temperature"}},
    {"Humidity", MENU_VALUE, {.state_key = "sensor.humidity"}},
    {"Settings", MENU_SUBMENU, {.submenu_id = 1}},
    // ...
};
```

### 3. Python генератор

```python
#!/usr/bin/env python3
# ui_generator.py

import json
import os
from pathlib import Path
from jinja2 import Template

class UIGenerator:
    def __init__(self, components_dir, output_dir):
        self.components_dir = Path(components_dir)
        self.output_dir = Path(output_dir)
        self.modules = {}
        
    def scan_modules(self):
        """Знайти всі ui_schema.json файли"""
        for schema_file in self.components_dir.glob("*/ui_schema.json"):
            with open(schema_file) as f:
                module_name = schema_file.parent.name
                self.modules[module_name] = json.load(f)
                
    def generate_web_ui(self):
        """Генерувати HTML/JS/CSS"""
        # Шаблон HTML
        html_template = Template('''
<!DOCTYPE html>
<html>
<head>
    <title>{{ title }}</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
</head>
<body>
    <div id="app">
        {% for module, schema in modules.items() %}
        <div class="module" id="{{ module }}">
            <h2>{{ schema.get('label', module) }}</h2>
            {% for control in schema.get('controls', []) %}
            {{ render_control(control) }}
            {% endfor %}
        </div>
        {% endfor %}
    </div>
</body>
</html>
        ''')
        
        # Генерація компактного HTML
        html = self.minify_html(html_template.render(
            title="ModESP Control",
            modules=self.modules,
            render_control=self.render_control
        ))
        
        # Генерація заголовкового файлу
        self.write_header_file('web_ui.h', {
            'INDEX_HTML': html,
            'APP_JS': self.generate_js(),
            'STYLE_CSS': self.generate_css(),
            'UI_ELEMENTS': self.generate_ui_elements()
        })
        
    def generate_mqtt_topics(self):
        """Генерувати MQTT топіки"""
        topics = []
        commands = []
        
        for module, schema in self.modules.items():
            # Телеметрія
            for key, config in schema.get('telemetry', {}).items():
                topic = f"modesp/{module}/{key}"
                topics.append({
                    'define': f"MQTT_TOPIC_{module.upper()}_{key.upper()}",
                    'topic': topic,
                    'state_key': config['source'],
                    'interval': config.get('interval', 60)
                })
            
            # Команди
            for control in schema.get('controls', []):
                if 'write_method' in control:
                    topic = f"modesp/{module}/{control['id']}/set"
                    commands.append({
                        'topic': topic,
                        'method': control['write_method']
                    })
        
        self.write_header_file('mqtt_topics.h', {
            'topics': topics,
            'commands': commands
        })
        
    def generate_lcd_menu(self):
        """Генерувати структуру LCD меню"""
        menu_items = []
        
        for module, schema in self.modules.items():
            # Головне меню - модулі
            submenu_items = []
            
            for control in schema.get('controls', []):
                if control['type'] in ['gauge', 'value']:
                    submenu_items.append({
                        'label': control['label'],
                        'type': 'MENU_VALUE',
                        'state_key': f"{module}.{control['id']}"
                    })
                    
            menu_items.append({
                'label': schema.get('label', module),
                'type': 'MENU_SUBMENU',
                'items': submenu_items
            })
            
        self.write_header_file('lcd_menu.h', {'menu': menu_items})
        
    def generate_modbus_map(self):
        """Генерувати Modbus register map"""
        registers = []
        address = 40001  # Holding registers
        
        for module, schema in self.modules.items():
            for control in schema.get('controls', []):
                if control['type'] in ['gauge', 'value', 'number']:
                    registers.append({
                        'address': address,
                        'name': f"{module}_{control['id']}",
                        'type': 'float' if control.get('unit') else 'uint16',
                        'scale': 10 if control.get('unit') == '%' else 1,
                        'rpc_method': control.get('read_method'),
                        'writable': 'write_method' in control
                    })
                    address += 2 if control.get('type') == 'float' else 1
                    
        self.write_header_file('modbus_map.h', {'registers': registers})

if __name__ == '__main__':
    generator = UIGenerator(
        components_dir='components',
        output_dir='main/generated'
    )
    generator.scan_modules()
    generator.generate_web_ui()
    generator.generate_mqtt_topics()
    generator.generate_lcd_menu()
    generator.generate_modbus_map()
```

### 4. CMake інтеграція

```cmake
# В головному CMakeLists.txt

# Запуск генератора перед компіляцією
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/main/generated/web_ui.h
           ${CMAKE_CURRENT_SOURCE_DIR}/main/generated/mqtt_topics.h
           ${CMAKE_CURRENT_SOURCE_DIR}/main/generated/lcd_menu.h
           ${CMAKE_CURRENT_SOURCE_DIR}/main/generated/modbus_map.h
    COMMAND ${Python3_EXECUTABLE} 
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/ui_generator.py
    DEPENDS ${UI_SCHEMA_FILES}
    COMMENT "Generating UI files from schemas..."
)

# Додати згенеровані файли до цілей
add_custom_target(generate_ui ALL
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/main/generated/web_ui.h
)

# Залежність для основного проекту
add_dependencies(${COMPONENT_LIB} generate_ui)
```

### 5. Runtime частина (мінімальна)

```cpp
// web_ui_module.cpp - тільки обслуговування
#include "generated/web_ui.h"

esp_err_t WebUIModule::handle_static_file(httpd_req_t *req) {
    if (strcmp(req->uri, "/") == 0) {
        // HTML вже згенерований і в PROGMEM
        httpd_resp_set_type(req, "text/html");
        return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    }
    // Аналогічно для JS/CSS
}

esp_err_t WebUIModule::handle_api_data(httpd_req_t *req) {
    // Тільки дані, без генерації UI
    json response;
    
    // Проходимо по таблиці елементів
    for (size_t i = 0; i < UI_ELEMENTS_COUNT; i++) {
        UIElement elem;
        memcpy_P(&elem, &UI_ELEMENTS[i], sizeof(UIElement));
        
        // Виклик RPC методу
        json value;
        api_dispatcher_->execute(elem.rpc_method, {}, value);
        response[elem.id] = value;
    }
    
    return send_json_response(req, response);
}
```

## Переваги підходу

### 1. Економія ресурсів
- **RAM**: HTML/JS/CSS в PROGMEM (Flash)
- **CPU**: Немає парсингу JSON схем в runtime
- **Розмір коду**: Мінімальний runtime код

### 2. Швидкодія
- Статичні файли віддаються напряму з Flash
- Немає генерації HTML при кожному запиті
- Таблиці в PROGMEM для швидкого доступу

### 3. Гнучкість
- Динамічні дані оновлюються через API
- Можливість runtime конфігурації (enable/disable модулів)
- Легко додавати нові модулі - перекомпіляція згенерує UI

### 4. Оптимізація розміру
- Мінімізація HTML/JS/CSS
- Видалення невикористаних елементів
- Compression для Web файлів

## Приклад використання

```bash
# Додаємо новий модуль
components/
  humidity_control/
    ui_schema.json    # Опис UI
    humidity_control.cpp

# Запускаємо збірку
idf.py build

# Генератор автоматично:
# 1. Знайде ui_schema.json
# 2. Згенерує всі UI файли
# 3. Скомпілює прошивку
```

## Runtime динаміка

Для елементів, що змінюються в runtime:

```cpp
// Приховування/показ елементів на основі конфігурації
class RuntimeUIConfig {
    std::bitset<MAX_UI_ELEMENTS> enabled_elements;
    
    void update_from_config(const json& config) {
        // Оновлюємо видимість елементів
        for (const auto& [module, enabled] : config.items()) {
            set_module_visibility(module, enabled);
        }
    }
};
```

Цей підхід забезпечує оптимальний баланс між гнучкістю та продуктивністю!