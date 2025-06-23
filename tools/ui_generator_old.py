#!/usr/bin/env python3
"""
UI Generator for ModESP
Generates static UI files from module schemas at compile time
"""

import json
import os
from pathlib import Path
from datetime import datetime

class UIGenerator:
    def __init__(self, components_dir='components', output_dir='main/generated'):
        self.components_dir = Path(components_dir)
        self.output_dir = Path(output_dir)
        self.modules = {}
        
        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
    def scan_modules(self):
        """Scan for ui_schema.json files in components"""
        print("Scanning for UI schemas...")
        
        for schema_file in self.components_dir.glob("*/ui_schema.json"):
            try:
                with open(schema_file, 'r', encoding='utf-8') as f:
                    module_name = schema_file.parent.name
                    self.modules[module_name] = json.load(f)
                    print(f"  Found schema for: {module_name}")
            except Exception as e:
                print(f"  Error reading {schema_file}: {e}")
                
        print(f"Total modules found: {len(self.modules)}")
        
    def generate_all(self):
        """Generate all UI files"""
        if not self.modules:
            print("No modules found. Skipping generation.")
            return
            
        self.generate_web_ui_header()
        self.generate_mqtt_topics_header()
        self.generate_lcd_menu_header()
        self.generate_ui_registry_header()
        
        print(f"\nGeneration complete! Files written to: {self.output_dir}")
        
    def generate_web_ui_header(self):
        """Generate web_ui_generated.h with HTML/JS/CSS"""
        print("\nGenerating web_ui_generated.h...")
        
        # Generate HTML
        html_parts = ['<!DOCTYPE html><html><head>',
                     '<meta charset="UTF-8">',
                     '<meta name="viewport" content="width=device-width,initial-scale=1">',
                     '<title>ModESP Control Panel</title>',
                     '<style>']
        
        # Basic CSS
        css = '''body{font-family:sans-serif;margin:0;padding:20px;background:#f0f0f0}
.module{background:white;border-radius:8px;padding:20px;margin-bottom:20px;box-shadow:0 2px 4px rgba(0,0,0,0.1)}
.module h2{margin-top:0;color:#333}
.control{margin:15px 0}
.control label{display:block;margin-bottom:5px;font-weight:bold}
.gauge{width:200px;height:100px;background:#eee;border-radius:4px;position:relative}
.value{font-size:24px;text-align:center;padding:20px}
input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px}
.switch{width:60px;height:30px;background:#ccc;border-radius:15px;position:relative;cursor:pointer}
.switch.on{background:#4CAF50}'''
        
        html_parts.append(css)
        html_parts.append('</style></head><body><div id="app">')
        
        # Generate module sections
        for module_name, schema in self.modules.items():
            html_parts.append(f'<div class="module" data-module="{module_name}">')
            html_parts.append(f'<h2>{schema.get("label", module_name)}</h2>')
            
            for control in schema.get('controls', []):
                html_parts.append(self._generate_control_html(control))
                
            html_parts.append('</div>')
            
        html_parts.append('</div>')  # app
        
        # Basic JavaScript
        js = '''<script>
// Auto-generated UI updater
const API_ENDPOINT = '/api/data';
const UPDATE_INTERVAL = 1000;

function updateUI() {
    fetch(API_ENDPOINT)
        .then(r => r.json())
        .then(data => {
            for (const [key, value] of Object.entries(data)) {
                const el = document.querySelector(`[data-bind="${key}"]`);
                if (el) {
                    if (el.tagName === 'INPUT' || el.tagName === 'SELECT') {
                        el.value = value;
                    } else {
                        el.textContent = value;
                    }
                }
            }
        })
        .catch(err => console.error('Update failed:', err));
}

// Handle control changes
document.addEventListener('change', (e) => {
    if (e.target.dataset.writeMethod) {
        const method = e.target.dataset.writeMethod;
        const value = e.target.type === 'checkbox' ? e.target.checked : e.target.value;
        
        fetch('/api/rpc', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                method: method,
                params: {value: value}
            })
        });
    }
});

// Start updates
setInterval(updateUI, UPDATE_INTERVAL);
updateUI();
</script>'''
        
        html_parts.append(js)
        html_parts.append('</body></html>')
        
        # Minify HTML
        html = ''.join(html_parts)
        
        # Write header file
        with open(self.output_dir / 'web_ui_generated.h', 'w') as f:
            f.write(f'''// Auto-generated by ui_generator.py
// Generated at: {datetime.now().isoformat()}
// DO NOT EDIT - changes will be overwritten

#pragma once

#include <pgmspace.h>

// Complete HTML page
const char INDEX_HTML[] PROGMEM = R"rawhtml({html})rawhtml";

// UI element mapping
struct UIElementMap {{
    const char* element_id;
    const char* rpc_method;
    const char* state_key;
}};

const UIElementMap UI_ELEMENT_MAP[] PROGMEM = {{
''')
            
            # Generate element map
            for module_name, schema in self.modules.items():
                for control in schema.get('controls', []):
                    element_id = f"{module_name}_{control['id']}"
                    read_method = control.get('read_method', '')
                    state_key = f"{module_name}.{control['id']}"
                    f.write(f'    {{"{element_id}", "{read_method}", "{state_key}"}},\n')
                    
            f.write('''};

const size_t UI_ELEMENT_COUNT = sizeof(UI_ELEMENT_MAP) / sizeof(UIElementMap);
''')
            
    def _generate_control_html(self, control):
        """Generate HTML for a single control"""
        control_id = control['id']
        control_type = control['type']
        label = control.get('label', control_id)
        
        html = f'<div class="control">'
        html += f'<label>{label}</label>'
        
        if control_type == 'gauge':
            html += f'<div class="gauge" data-bind="{control_id}">'
            html += f'<div class="value">--</div>'
            html += '</div>'
        elif control_type == 'value':
            html += f'<div class="value" data-bind="{control_id}">--</div>'
        elif control_type == 'number':
            html += f'<input type="number" data-bind="{control_id}"'
            if 'min' in control:
                html += f' min="{control["min"]}"'
            if 'max' in control:
                html += f' max="{control["max"]}"'
            if 'step' in control:
                html += f' step="{control["step"]}"'
            if 'write_method' in control:
                html += f' data-write-method="{control["write_method"]}"'
            html += '>'
        elif control_type == 'switch':
            html += f'<div class="switch" data-bind="{control_id}"'
            if 'write_method' in control:
                html += f' data-write-method="{control["write_method"]}"'
            html += '></div>'
        elif control_type == 'select':
            html += f'<select data-bind="{control_id}"'
            if 'write_method' in control:
                html += f' data-write-method="{control["write_method"]}"'
            html += '>'
            for value, text in control.get('options', {}).items():
                html += f'<option value="{value}">{text}</option>'
            html += '</select>'
            
        html += '</div>'
        return html
    def generate_mqtt_topics_header(self):
        """Generate mqtt_topics_generated.h"""
        print("Generating mqtt_topics_generated.h...")
        
        with open(self.output_dir / 'mqtt_topics_generated.h', 'w') as f:
            f.write(f'''// Auto-generated MQTT topic definitions
// Generated at: {datetime.now().isoformat()}

#pragma once

#include <pgmspace.h>

''')
            
            # Generate topic defines
            for module_name, schema in self.modules.items():
                f.write(f'// {module_name} topics\n')
                
                # Telemetry topics
                for key, config in schema.get('telemetry', {}).items():
                    define_name = f'MQTT_TOPIC_{module_name.upper()}_{key.upper()}'
                    topic = f'modesp/{module_name}/{key}'
                    f.write(f'#define {define_name} "{topic}"\n')
                    
                # Command topics
                for control in schema.get('controls', []):
                    if 'write_method' in control:
                        define_name = f'MQTT_CMD_{module_name.upper()}_{control["id"].upper()}'
                        topic = f'modesp/{module_name}/{control["id"]}/set'
                        f.write(f'#define {define_name} "{topic}"\n')
                        
                f.write('\n')
                
            # Generate telemetry mapping
            f.write('''// Telemetry mapping
struct MqttTelemetryMap {
    const char* topic;
    const char* state_key;
    uint16_t interval_s;
};

const MqttTelemetryMap MQTT_TELEMETRY_MAP[] PROGMEM = {
''')
            
            for module_name, schema in self.modules.items():
                for key, config in schema.get('telemetry', {}).items():
                    topic_define = f'MQTT_TOPIC_{module_name.upper()}_{key.upper()}'
                    state_key = config.get('source', f'{module_name}.{key}')
                    interval = config.get('interval', 60)
                    f.write(f'    {{{topic_define}, "{state_key}", {interval}}},\n')
                    
            f.write('''};

const size_t MQTT_TELEMETRY_COUNT = sizeof(MQTT_TELEMETRY_MAP) / sizeof(MqttTelemetryMap);
''')
            
    def generate_lcd_menu_header(self):
        """Generate lcd_menu_generated.h"""
        print("Generating lcd_menu_generated.h...")
        
        with open(self.output_dir / 'lcd_menu_generated.h', 'w') as f:
            f.write(f'''// Auto-generated LCD menu structure
// Generated at: {datetime.now().isoformat()}

#pragma once

#include <pgmspace.h>

enum MenuItemType {
    MENU_TYPE_VALUE,
    MENU_TYPE_SUBMENU,
    MENU_TYPE_ACTION,
    MENU_TYPE_BACK
};

struct MenuItem {
    const char* label;
    MenuItemType type;
    union {
        const char* state_key;     // for values
        uint8_t submenu_id;        // for submenus
        uint8_t action_id;         // for actions
    } data;
};

''')
            
            # Generate submenus for each module
            submenu_id = 1
            for module_name, schema in self.modules.items():
                f.write(f'// {module_name} submenu\n')
                f.write(f'const MenuItem SUBMENU_{submenu_id}[] PROGMEM = {{\n')
                
                # Add value displays
                for control in schema.get('controls', []):
                    if control['type'] in ['gauge', 'value', 'number']:
                        label = control.get('label', control['id'])[:16]  # LCD limit
                        state_key = f'{module_name}.{control["id"]}'
                        f.write(f'    {{"{label}", MENU_TYPE_VALUE, {{.state_key = "{state_key}"}}}},\n')
                        
                f.write(f'    {{"Back", MENU_TYPE_BACK, {{0}}}},\n')
                f.write(f'}};\n\n')
                submenu_id += 1
                
            # Generate main menu
            f.write('// Main menu\n')
            f.write('const MenuItem MAIN_MENU[] PROGMEM = {\n')
            
            submenu_id = 1
            for module_name, schema in self.modules.items():
                label = schema.get('label', module_name)[:16]
                f.write(f'    {{"{label}", MENU_TYPE_SUBMENU, {{.submenu_id = {submenu_id}}}}},\n')
                submenu_id += 1
                
            f.write('};\n\n')
            f.write('const size_t MAIN_MENU_SIZE = sizeof(MAIN_MENU) / sizeof(MenuItem);\n')
            
    def generate_ui_registry_header(self):
        """Generate ui_registry_generated.h with module UI metadata"""
        print("Generating ui_registry_generated.h...")
        
        with open(self.output_dir / 'ui_registry_generated.h', 'w') as f:
            f.write(f'''// Auto-generated UI registry
// Generated at: {datetime.now().isoformat()}

#pragma once

#include <pgmspace.h>

// Module capabilities flags
#define UI_CAP_READ      0x01
#define UI_CAP_WRITE     0x02
#define UI_CAP_TELEMETRY 0x04
#define UI_CAP_ALARMS    0x08

struct ModuleUIInfo {
    const char* name;
    const char* label;
    uint8_t capabilities;
    uint16_t update_rate_ms;
};

const ModuleUIInfo MODULE_UI_REGISTRY[] PROGMEM = {
''')
            
            for module_name, schema in self.modules.items():
                label = schema.get('label', module_name)
                caps = self._calculate_capabilities(schema)
                update_rate = schema.get('update_rate_ms', 1000)
                
                f.write(f'    {{"{module_name}", "{label}", 0x{caps:02X}, {update_rate}}},\n')
                
            f.write('''};

const size_t MODULE_UI_COUNT = sizeof(MODULE_UI_REGISTRY) / sizeof(ModuleUIInfo);

// Helper macros
#define MODULE_HAS_CAP(module_idx, cap) \\
    (pgm_read_byte(&MODULE_UI_REGISTRY[module_idx].capabilities) & (cap))
    
#define GET_MODULE_NAME(module_idx) \\
    ((const char*)pgm_read_ptr(&MODULE_UI_REGISTRY[module_idx].name))
''')
            
    def _calculate_capabilities(self, schema):
        """Calculate capability flags from schema"""
        caps = 0
        
        # Check for read capability
        if any('read_method' in c for c in schema.get('controls', [])):
            caps |= 0x01  # UI_CAP_READ
            
        # Check for write capability
        if any('write_method' in c for c in schema.get('controls', [])):
            caps |= 0x02  # UI_CAP_WRITE
            
        # Check for telemetry
        if 'telemetry' in schema:
            caps |= 0x04  # UI_CAP_TELEMETRY
            
        # Check for alarms
        if 'alarms' in schema:
            caps |= 0x08  # UI_CAP_ALARMS
            
        return caps

if __name__ == '__main__':
    import sys
    
    # Default paths
    components_dir = 'components'
    output_dir = 'main/generated'
    
    # Override from command line
    if len(sys.argv) > 1:
        components_dir = sys.argv[1]
    if len(sys.argv) > 2:
        output_dir = sys.argv[2]
        
    # Run generator
    generator = UIGenerator(components_dir, output_dir)
    generator.scan_modules()
    generator.generate_all()