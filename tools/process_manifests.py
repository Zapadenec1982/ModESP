#!/usr/bin/env python3
"""
ModESP Manifest Processor
Processes module and driver manifests to generate C++ code for:
- API registration
- UI schemas  
- Event handlers
- Configuration schemas
"""

import json
import os
import sys
import argparse
import logging
from pathlib import Path
from typing import Dict, List, Any, Optional
from datetime import datetime
import jsonschema
from dataclasses import dataclass, field
from adaptive_ui_generator import generate_adaptive_ui
from manifest_validator import ManifestValidator, ValidationIssue


@dataclass
class ModuleInfo:
    """Parsed module information from manifest"""
    name: str
    type: str
    version: str
    description: str = ""
    priority: str = "NORMAL"
    dependencies: List[str] = field(default_factory=list)
    apis: List[Dict] = field(default_factory=list)
    ui_pages: List[Dict] = field(default_factory=list)
    shared_state_keys: List[str] = field(default_factory=list)
    events: List[str] = field(default_factory=list)
    config_file: Optional[str] = None
    manifest_path: str = ""
    ui: Dict = field(default_factory=dict)  # Full UI data for adaptive UI
    driver_interface: Optional[str] = None  # For MANAGER type modules


class ManifestProcessor:
    """Main processor for module manifests"""
    
    def __init__(self, project_root: str, output_dir: str):
        self.project_root = Path(project_root)
        self.output_dir = Path(output_dir)
        self.components_dir = self.project_root / "components"
        self.schemas_dir = self.project_root / "tools" / "manifest_schemas"
        
        # Load schemas
        self.module_schema = self._load_schema("module-manifest.schema.json")
        self.driver_schema = self._load_schema("driver-manifest.schema.json")
        
        # Collections
        self.modules: List[ModuleInfo] = []
        self.drivers: List[Dict] = []
        self.all_apis: List[Dict] = []
        self.all_ui_schemas: Dict[str, Any] = {}
        self.all_events: Dict[str, List[str]] = {}
        
        # Setup logging
        logging.basicConfig(
            level=logging.INFO,
            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
        )
        self.logger = logging.getLogger("ManifestProcessor")
        
    def _load_schema(self, filename: str) -> Dict:
        """Load JSON schema from file"""
        schema_path = self.schemas_dir / filename
        if not schema_path.exists():
            self.logger.warning(f"Schema file not found: {schema_path}")
            return {}
            
        with open(schema_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    
    def process_all(self):
        """Main processing pipeline"""
        self.logger.info("Starting manifest processing...")
        
        # Step 1: Discover manifests
        self.discover_manifests()
        
        # Step 2: Parse and validate manifests
        self.parse_manifests()
        
        # Step 3: Validate consistency across manifests
        if not self.validate_manifest_consistency():
            self.logger.error("Manifest validation failed! Fix errors before proceeding.")
            sys.exit(1)
        
        # Step 4: Resolve dependencies
        self.resolve_dependencies()
        
        # Step 5: Generate code
        self.generate_code()
        
        self.logger.info("Manifest processing complete!")
    
    def validate_manifest_consistency(self) -> bool:
        """Validate consistency across all manifests"""
        self.logger.info("Validating manifest consistency...")
        
        # Create validator
        validator = ManifestValidator()
        
        # Prepare manifest data for validation
        all_manifests = []
        for module in self.modules:
            manifest = self._module_to_dict(module)
            
            # Add event_bus data if exists
            manifest['event_bus'] = {
                'publishes': {},
                'subscribes': {}
            }
            
            # Collect events from module
            for event in module.events:
                if event in self.all_events:
                    manifest['event_bus']['publishes'][event] = self.all_events[event]
            
            # Add shared_state data if available
            manifest['shared_state'] = module.shared_state
            
            all_manifests.append(manifest)
        
        # Run validation
        is_valid, issues = validator.validate_all(all_manifests)
        
        # Generate validation report
        report = validator.generate_validation_report()
        
        # Save report
        report_file = self.project_root / "build" / "manifest_validation_report.md"
        report_file.parent.mkdir(parents=True, exist_ok=True)
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write(report)
        
        # Log results
        if issues:
            self.logger.info(f"Validation found {len(issues)} issues:")
            for issue in issues:
                if issue.severity == "ERROR":
                    self.logger.error(f"  {issue.category}: {issue.message}")
                elif issue.severity == "WARNING":
                    self.logger.warning(f"  {issue.category}: {issue.message}")
                else:
                    self.logger.info(f"  {issue.category}: {issue.message}")
            
            self.logger.info(f"Validation report saved to: {report_file}")
        else:
            self.logger.info("✅ All validations passed!")
        
        return is_valid
    
    def _module_to_dict(self, module: ModuleInfo) -> Dict:
        """Convert ModuleInfo to dict for adaptive UI generator"""
        return {
            'module': {
                'name': module.name,
                'type': module.type,
                'version': module.version,
                'description': module.description,
                'priority': module.priority,
                'driver_interface': module.driver_interface
            },
            'ui': module.ui,
            'apis': module.apis
        }
    
    def discover_manifests(self):
        """Find all manifest files in the project"""
        self.logger.info("Discovering manifests...")
        
        # Find module manifests
        module_manifests = list(self.components_dir.rglob("module_manifest.json"))
        self.logger.info(f"Found {len(module_manifests)} module manifests")
        
        # Find driver manifests  
        driver_manifests = list(self.components_dir.rglob("*_driver_manifest.json"))
        self.logger.info(f"Found {len(driver_manifests)} driver manifests")
        
        self.manifest_files = {
            'modules': module_manifests,
            'drivers': driver_manifests
        }
    
    def parse_manifests(self):
        """Parse and validate all manifests"""
        # Parse module manifests
        for manifest_path in self.manifest_files['modules']:
            try:
                self.logger.info(f"Parsing module manifest: {manifest_path}")
                module = self.parse_module_manifest(manifest_path)
                if module:
                    self.modules.append(module)
            except Exception as e:
                self.logger.error(f"Error parsing {manifest_path}: {e}")
        
        # Parse driver manifests
        for manifest_path in self.manifest_files['drivers']:
            try:
                self.logger.info(f"Parsing driver manifest: {manifest_path}")
                driver = self.parse_driver_manifest(manifest_path)
                if driver:
                    self.drivers.append(driver)
            except Exception as e:
                self.logger.error(f"Error parsing {manifest_path}: {e}")
    
    def parse_module_manifest(self, manifest_path: Path) -> Optional[ModuleInfo]:
        """Parse a single module manifest"""
        with open(manifest_path, 'r', encoding='utf-8') as f:
            manifest = json.load(f)
        
        # Validate against schema
        if self.module_schema:
            try:
                jsonschema.validate(manifest, self.module_schema)
            except jsonschema.ValidationError as e:
                self.logger.error(f"Schema validation failed for {manifest_path}: {e}")
                return None
        
        # Extract module info
        module_data = manifest.get('module', {})
        module = ModuleInfo(
            name=module_data.get('name', ''),
            type=module_data.get('type', 'STANDARD'),
            version=module_data.get('version', '0.0.0'),
            description=module_data.get('description', ''),
            priority=module_data.get('priority', 'NORMAL'),
            dependencies=module_data.get('dependencies', []),
            manifest_path=str(manifest_path),
            driver_interface=module_data.get('driver_interface', None)
        )
        
        # Extract APIs
        if 'apis' in manifest:
            static_apis = manifest['apis'].get('static', [])
            for api in static_apis:
                api['module'] = module.name
                self.all_apis.append(api)
                module.apis.append(api)
        
        # Extract UI schemas
        if 'ui' in manifest:
            ui_data = manifest['ui']
            # Store full UI data for adaptive UI
            module.ui = ui_data
            if 'static' in ui_data and 'pages' in ui_data['static']:
                module.ui_pages = ui_data['static']['pages']
                self.all_ui_schemas[module.name] = ui_data
        
        # Extract shared state keys
        if 'shared_state' in manifest:
            publishes = manifest['shared_state'].get('publishes', {})
            module.shared_state_keys.extend(publishes.keys())
        
        # Extract events
        if 'event_bus' in manifest:
            publishes = manifest['event_bus'].get('publishes', {})
            module.events.extend(publishes.keys())
            for event in publishes:
                if event not in self.all_events:
                    self.all_events[event] = []
                self.all_events[event].append(module.name)
        
        # Configuration
        if 'configuration' in manifest:
            module.config_file = manifest['configuration'].get('config_file')
        
        return module
    
    def parse_driver_manifest(self, manifest_path: Path) -> Optional[Dict]:
        """Parse a single driver manifest"""
        with open(manifest_path, 'r', encoding='utf-8') as f:
            manifest = json.load(f)
        
        # Validate against schema
        if self.driver_schema:
            try:
                jsonschema.validate(manifest, self.driver_schema)
            except jsonschema.ValidationError as e:
                self.logger.error(f"Schema validation failed for {manifest_path}: {e}")
                return None
        
        driver_data = manifest.get('driver', {})
        driver_data['manifest_path'] = str(manifest_path)
        driver_data['apis'] = manifest.get('apis', {}).get('specific', [])
        
        return driver_data
    
    def resolve_dependencies(self):
        """Check and resolve module dependencies"""
        self.logger.info("Resolving dependencies...")
        
        module_names = {m.name for m in self.modules}
        
        for module in self.modules:
            for dep in module.dependencies:
                if dep not in module_names and dep not in ['ESPhal', 'SharedState', 'EventBus']:
                    self.logger.warning(f"Module {module.name} depends on unknown module: {dep}")
    
    def generate_code(self):
        """Generate C++ code from parsed manifests"""
        self.logger.info("Generating code...")
        
        # Ensure output directory exists
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Generate files
        self.generate_api_registry()
        self.generate_module_info()
        # self.generate_ui_schemas()  # Phase 2 - deprecated
        self.generate_event_registry()
        self.generate_system_contract_documentation()  # Generate docs separately
        
        # Generate adaptive UI components
        self.logger.info("Generating adaptive UI components...")
        generate_adaptive_ui(
            [self._module_to_dict(m) for m in self.modules],
            self.drivers,
            self.output_dir
        )
        
    def generate_api_registry(self):
        """Generate API registry C++ code"""
        output_file = self.output_dir / "generated_api_registry.cpp"
        
        self.logger.info(f"Generating {output_file}")
        
        code = [
            "// AUTO-GENERATED FILE - DO NOT EDIT",
            f"// Generated by process_manifests.py on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            '#include "generated_api_registry.h"',
            '#include "json_rpc_interface.h"',
            "",
            "namespace ModESP {",
            "",
            "void register_generated_apis(JsonRpcInterface& rpc) {",
        ]
        
        # Group APIs by module
        for module in self.modules:
            if module.apis:
                code.append(f"    // APIs from {module.name}")
                for api in module.apis:
                    method = api.get('method', '')
                    handler = api.get('handler', '')
                    access_level = api.get('access_level', 'user')
                    
                    code.append(f'    rpc.register_method("{method}", &{handler}, AccessLevel::{access_level.upper()});')
                code.append("")
        
        code.extend([
            "}",
            "",
            "} // namespace ModESP"
        ])
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(code))
    
    def generate_system_contract_documentation(self):
        """Generate comprehensive system contract documentation"""
        # Create system_contract directory
        contract_dir = self.project_root / "components" / "system_contract"
        contract_dir.mkdir(parents=True, exist_ok=True)
        
        self.logger.info(f"Generating system contract documentation in {contract_dir}")
        
        # Generate main README
        self._generate_contract_readme(contract_dir)
        
        # Generate events documentation
        self._generate_events_doc(contract_dir)
        
        # Generate states documentation  
        self._generate_states_doc(contract_dir)
        
        # Generate examples
        self._generate_contract_examples(contract_dir)
    
    def _generate_contract_readme(self, contract_dir):
        """Generate main README for system contract"""
        output_file = contract_dir / "README.md"
        
        content = [
            "# System Contract Documentation",
            "",
            "This directory contains the complete contract documentation for inter-module communication in ModESP.",
            "",
            "## 📚 Contents",
            "",
            "- **[events.md](events.md)** - All system events with payload structures",
            "- **[states.md](states.md)** - All SharedState keys with types and update rates", 
            "- **[examples/](examples/)** - Usage examples for common scenarios",
            "",
            "## 🎯 Quick Reference",
            "",
            "### Using Events",
            "```cpp",
            '#include "generated_system_contract.h"',
            "",
            "// Subscribe to sensor updates",
            "EventBus::subscribe(Events::SENSOR_READING_UPDATED, handler);",
            "",
            "// Publish error",
            'EventBus::publish(Events::SENSOR_ERROR, {{"message", "Timeout"}});',
            "```",
            "",
            "### Using SharedState",
            "```cpp",
            "// Read temperature",
            "float temp = SharedState::get<float>(States::TEMP_EVAPORATOR);",
            "",
            "// Update state",
            'SharedState::set(States::CLIMATE_MODE, "cooling");',
            "```",
            "",
            f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"Total events: {len(self.all_events)}",
            f"Total modules: {len(self.modules)}"
        ]
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_events_doc(self, contract_dir):
        """Generate detailed events documentation"""
        output_file = contract_dir / "events.md"
        
        content = [
            "# System Events Documentation",
            "",
            "Complete reference of all events in the ModESP system.",
            "",
        ]
        
        # Group events by module
        events_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'event_bus' in manifest and 'publishes' in manifest['event_bus']:
                module_name = module.name
                events_by_module[module_name] = manifest['event_bus']['publishes']
        
        # Generate documentation for each module
        for module_name, events in sorted(events_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for event_name, event_info in sorted(events.items()):
                const_name = event_name.upper().replace('.', '_')
                content.append(f"### `{event_name}`")
                content.append(f"**Constant**: `Events::{const_name}`")
                content.append("")
                
                if 'description' in event_info:
                    content.append(f"**Description**: {event_info['description']}")
                    content.append("")
                
                if 'payload' in event_info:
                    content.append("**Payload**:")
                    for field, field_info in event_info['payload'].items():
                        field_type = field_info.get('type', 'unknown')
                        field_desc = field_info.get('description', '')
                        content.append(f"- `{field}` ({field_type}): {field_desc}")
                    content.append("")
                
                content.append("**Example**:")
                content.append("```cpp")
                content.append(f'EventBus::publish(Events::{const_name}, {{')
                if 'payload' in event_info:
                    for field in list(event_info['payload'].keys())[:2]:
                        content.append(f'    {{"{field}", value}},')
                content.append('});')
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_states_doc(self, contract_dir):
        """Generate detailed SharedState documentation"""
        output_file = contract_dir / "states.md"
        
        content = [
            "# SharedState Keys Documentation",
            "",
            "Complete reference of all SharedState keys in the ModESP system.",
            "",
        ]
        
        # Group states by module
        states_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'shared_state' in manifest and 'publishes' in manifest['shared_state']:
                module_name = module.name
                states_by_module[module_name] = manifest['shared_state']['publishes']
        
        # Generate documentation for each module
        for module_name, states in sorted(states_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for state_key, state_info in sorted(states.items()):
                const_name = state_key.upper().replace('.', '_')
                content.append(f"### `{state_key}`")
                content.append(f"**Constant**: `States::{const_name}`")
                content.append("")
                
                content.append(f"**Type**: `{state_info.get('type', 'unknown')}`")
                
                if 'description' in state_info:
                    content.append(f"**Description**: {state_info['description']}")
                
                if 'update_rate_ms' in state_info:
                    content.append(f"**Update Rate**: {state_info['update_rate_ms']}ms")
                
                content.append("")
                content.append("**Example**:")
                content.append("```cpp")
                
                state_type = state_info.get('type', 'auto')
                if state_type == 'float' or state_type == 'number':
                    content.append(f"float value = SharedState::get<float>(States::{const_name});")
                elif state_type == 'boolean':
                    content.append(f"bool value = SharedState::get<bool>(States::{const_name});")
                else:
                    content.append(f"auto value = SharedState::get(States::{const_name});")
                
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_contract_examples(self, contract_dir):
        """Generate example code for common scenarios"""
        examples_dir = contract_dir / "examples"
        examples_dir.mkdir(parents=True, exist_ok=True)
        
        # Temperature monitoring example
        temp_example = examples_dir / "temperature_monitoring.cpp"
        with open(temp_example, 'w', encoding='utf-8') as f:
            f.write('''/**
 * @file temperature_monitoring.cpp
 * @brief Example: Monitoring temperature sensors
 */

#include "generated_system_contract.h"
#include "event_bus.h"
#include "shared_state.h"

class TemperatureMonitor {
public:
    void init() {
        // Subscribe to temperature updates
        EventBus::subscribe(Events::SENSOR_READING_UPDATED, 
            [this](const EventBus::Event& event) {
                auto sensor_role = event.data["sensor_role"].get<std::string>();
                
                if (sensor_role == "temperature_evaporator") {
                    float temp = event.data["value"];
                    onEvaporatorTempChanged(temp);
                }
            });
    }
    
    void checkCurrentTemperature() {
        // Read current temperature from SharedState
        float evap_temp = SharedState::get<float>(States::TEMP_EVAPORATOR);
        float ambient_temp = SharedState::get<float>(States::TEMP_AMBIENT);
        
        ESP_LOGI(TAG, "Evaporator: %.1f°C, Ambient: %.1f°C", 
                 evap_temp, ambient_temp);
    }
    
private:
    void onEvaporatorTempChanged(float temp) {
        if (temp < -30.0f) {
            // Publish warning event
            EventBus::publish(Events::SYSTEM_HEALTH_WARNING, {
                {"module", "temperature_monitor"},
                {"reason", "Evaporator too cold"},
                {"value", temp}
            });
        }
    }
    
    static constexpr const char* TAG = "TempMonitor";
};
''')
        
        self.logger.info(f"Generated contract documentation in {contract_dir}")
    
    def generate_module_info(self):
        """Generate module information C++ code"""
        output_file = self.output_dir / "generated_module_info.cpp"
        
        self.logger.info(f"Generating {output_file}")
        
        code = [
            "// AUTO-GENERATED FILE - DO NOT EDIT", 
            f"// Generated by process_manifests.py on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            '#include "generated_module_info.h"',
            "",
            "namespace ModESP {",
            "",
            "const ModuleInfo generated_modules[] = {",
        ]
        
        for module in self.modules:
            deps_str = ', '.join(f'"{dep}"' for dep in module.dependencies)
            config_str = f'"{module.config_file}"' if module.config_file else 'nullptr'
            
            code.extend([
                "    {",
                f'        .name = "{module.name}",',
                f'        .type = ModuleType::{module.type},',
                f'        .version = "{module.version}",',
                f'        .description = "{module.description}",',
                f'        .priority = Priority::{module.priority},',
                f'        .dependencies = {{{deps_str}}},',
                f'        .config_file = {config_str},',
                "    },",
            ])
        
        code.extend([
            "};",
            "",
            f"const size_t generated_module_count = {len(self.modules)};",
            "",
            "} // namespace ModESP"
        ])
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(code))
    
    def generate_system_contract_documentation(self):
        """Generate comprehensive system contract documentation"""
        # Create system_contract directory
        contract_dir = self.project_root / "components" / "system_contract"
        contract_dir.mkdir(parents=True, exist_ok=True)
        
        self.logger.info(f"Generating system contract documentation in {contract_dir}")
        
        # Generate main README
        self._generate_contract_readme(contract_dir)
        
        # Generate events documentation
        self._generate_events_doc(contract_dir)
        
        # Generate states documentation  
        self._generate_states_doc(contract_dir)
        
        # Generate examples
        self._generate_contract_examples(contract_dir)
    
    def _generate_contract_readme(self, contract_dir):
        """Generate main README for system contract"""
        output_file = contract_dir / "README.md"
        
        content = [
            "# System Contract Documentation",
            "",
            "This directory contains the complete contract documentation for inter-module communication in ModESP.",
            "",
            "## 📚 Contents",
            "",
            "- **[events.md](events.md)** - All system events with payload structures",
            "- **[states.md](states.md)** - All SharedState keys with types and update rates", 
            "- **[examples/](examples/)** - Usage examples for common scenarios",
            "",
            "## 🎯 Quick Reference",
            "",
            "### Using Events",
            "```cpp",
            '#include "generated_system_contract.h"',
            "",
            "// Subscribe to sensor updates",
            "EventBus::subscribe(Events::SENSOR_READING_UPDATED, handler);",
            "",
            "// Publish error",
            'EventBus::publish(Events::SENSOR_ERROR, {{"message", "Timeout"}});',
            "```",
            "",
            "### Using SharedState",
            "```cpp",
            "// Read temperature",
            "float temp = SharedState::get<float>(States::TEMP_EVAPORATOR);",
            "",
            "// Update state",
            'SharedState::set(States::CLIMATE_MODE, "cooling");',
            "```",
            "",
            f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"Total events: {len(self.all_events)}",
            f"Total modules: {len(self.modules)}"
        ]
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_events_doc(self, contract_dir):
        """Generate detailed events documentation"""
        output_file = contract_dir / "events.md"
        
        content = [
            "# System Events Documentation",
            "",
            "Complete reference of all events in the ModESP system.",
            "",
        ]
        
        # Group events by module
        events_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'event_bus' in manifest and 'publishes' in manifest['event_bus']:
                module_name = module.name
                events_by_module[module_name] = manifest['event_bus']['publishes']
        
        # Generate documentation for each module
        for module_name, events in sorted(events_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for event_name, event_info in sorted(events.items()):
                const_name = event_name.upper().replace('.', '_')
                content.append(f"### `{event_name}`")
                content.append(f"**Constant**: `Events::{const_name}`")
                content.append("")
                
                if 'description' in event_info:
                    content.append(f"**Description**: {event_info['description']}")
                    content.append("")
                
                if 'payload' in event_info:
                    content.append("**Payload**:")
                    for field, field_info in event_info['payload'].items():
                        field_type = field_info.get('type', 'unknown')
                        field_desc = field_info.get('description', '')
                        content.append(f"- `{field}` ({field_type}): {field_desc}")
                    content.append("")
                
                content.append("**Example**:")
                content.append("```cpp")
                content.append(f'EventBus::publish(Events::{const_name}, {{')
                if 'payload' in event_info:
                    for field in list(event_info['payload'].keys())[:2]:
                        content.append(f'    {{"{field}", value}},')
                content.append('});')
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_states_doc(self, contract_dir):
        """Generate detailed SharedState documentation"""
        output_file = contract_dir / "states.md"
        
        content = [
            "# SharedState Keys Documentation",
            "",
            "Complete reference of all SharedState keys in the ModESP system.",
            "",
        ]
        
        # Group states by module
        states_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'shared_state' in manifest and 'publishes' in manifest['shared_state']:
                module_name = module.name
                states_by_module[module_name] = manifest['shared_state']['publishes']
        
        # Generate documentation for each module
        for module_name, states in sorted(states_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for state_key, state_info in sorted(states.items()):
                const_name = state_key.upper().replace('.', '_')
                content.append(f"### `{state_key}`")
                content.append(f"**Constant**: `States::{const_name}`")
                content.append("")
                
                content.append(f"**Type**: `{state_info.get('type', 'unknown')}`")
                
                if 'description' in state_info:
                    content.append(f"**Description**: {state_info['description']}")
                
                if 'update_rate_ms' in state_info:
                    content.append(f"**Update Rate**: {state_info['update_rate_ms']}ms")
                
                content.append("")
                content.append("**Example**:")
                content.append("```cpp")
                
                state_type = state_info.get('type', 'auto')
                if state_type == 'float' or state_type == 'number':
                    content.append(f"float value = SharedState::get<float>(States::{const_name});")
                elif state_type == 'boolean':
                    content.append(f"bool value = SharedState::get<bool>(States::{const_name});")
                else:
                    content.append(f"auto value = SharedState::get(States::{const_name});")
                
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_contract_examples(self, contract_dir):
        """Generate example code for common scenarios"""
        examples_dir = contract_dir / "examples"
        examples_dir.mkdir(parents=True, exist_ok=True)
        
        # Temperature monitoring example
        temp_example = examples_dir / "temperature_monitoring.cpp"
        with open(temp_example, 'w', encoding='utf-8') as f:
            f.write('''/**
 * @file temperature_monitoring.cpp
 * @brief Example: Monitoring temperature sensors
 */

#include "generated_system_contract.h"
#include "event_bus.h"
#include "shared_state.h"

class TemperatureMonitor {
public:
    void init() {
        // Subscribe to temperature updates
        EventBus::subscribe(Events::SENSOR_READING_UPDATED, 
            [this](const EventBus::Event& event) {
                auto sensor_role = event.data["sensor_role"].get<std::string>();
                
                if (sensor_role == "temperature_evaporator") {
                    float temp = event.data["value"];
                    onEvaporatorTempChanged(temp);
                }
            });
    }
    
    void checkCurrentTemperature() {
        // Read current temperature from SharedState
        float evap_temp = SharedState::get<float>(States::TEMP_EVAPORATOR);
        float ambient_temp = SharedState::get<float>(States::TEMP_AMBIENT);
        
        ESP_LOGI(TAG, "Evaporator: %.1f°C, Ambient: %.1f°C", 
                 evap_temp, ambient_temp);
    }
    
private:
    void onEvaporatorTempChanged(float temp) {
        if (temp < -30.0f) {
            // Publish warning event
            EventBus::publish(Events::SYSTEM_HEALTH_WARNING, {
                {"module", "temperature_monitor"},
                {"reason", "Evaporator too cold"},
                {"value", temp}
            });
        }
    }
    
    static constexpr const char* TAG = "TempMonitor";
};
''')
        
        self.logger.info(f"Generated contract documentation in {contract_dir}")
    
    def generate_ui_schemas(self):
        """Generate UI schemas header file"""
        output_file = self.output_dir / "generated_ui_schemas.h"
        
        self.logger.info(f"Generating {output_file}")
        
        code = [
            "// AUTO-GENERATED FILE - DO NOT EDIT",
            f"// Generated by process_manifests.py on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            "#pragma once",
            "",
            '#include <string>',
            '#include <vector>',
            '#include <map>',
            "",
            "namespace ModESP {",
            "",
        ]
        code.extend([
            "struct UIPage {",
            "    std::string id;",
            "    std::string label;",
            "    std::string layout;",
            "    std::string access_level;",
            "    // TODO: Add widgets",
            "};",
            "",
            "struct ModuleUISchema {",
            "    std::string module_name;",
            "    std::vector<UIPage> pages;",
            "};",
            "",
            "extern const std::vector<ModuleUISchema> generated_ui_schemas;",
            "",
            "} // namespace ModESP"
        ])
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(code))
    
    def generate_system_contract_documentation(self):
        """Generate comprehensive system contract documentation"""
        # Create system_contract directory
        contract_dir = self.project_root / "components" / "system_contract"
        contract_dir.mkdir(parents=True, exist_ok=True)
        
        self.logger.info(f"Generating system contract documentation in {contract_dir}")
        
        # Generate main README
        self._generate_contract_readme(contract_dir)
        
        # Generate events documentation
        self._generate_events_doc(contract_dir)
        
        # Generate states documentation  
        self._generate_states_doc(contract_dir)
        
        # Generate examples
        self._generate_contract_examples(contract_dir)
    
    def _generate_contract_readme(self, contract_dir):
        """Generate main README for system contract"""
        output_file = contract_dir / "README.md"
        
        content = [
            "# System Contract Documentation",
            "",
            "This directory contains the complete contract documentation for inter-module communication in ModESP.",
            "",
            "## 📚 Contents",
            "",
            "- **[events.md](events.md)** - All system events with payload structures",
            "- **[states.md](states.md)** - All SharedState keys with types and update rates", 
            "- **[examples/](examples/)** - Usage examples for common scenarios",
            "",
            "## 🎯 Quick Reference",
            "",
            "### Using Events",
            "```cpp",
            '#include "generated_system_contract.h"',
            "",
            "// Subscribe to sensor updates",
            "EventBus::subscribe(Events::SENSOR_READING_UPDATED, handler);",
            "",
            "// Publish error",
            'EventBus::publish(Events::SENSOR_ERROR, {{"message", "Timeout"}});',
            "```",
            "",
            "### Using SharedState",
            "```cpp",
            "// Read temperature",
            "float temp = SharedState::get<float>(States::TEMP_EVAPORATOR);",
            "",
            "// Update state",
            'SharedState::set(States::CLIMATE_MODE, "cooling");',
            "```",
            "",
            f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"Total events: {len(self.all_events)}",
            f"Total modules: {len(self.modules)}"
        ]
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_events_doc(self, contract_dir):
        """Generate detailed events documentation"""
        output_file = contract_dir / "events.md"
        
        content = [
            "# System Events Documentation",
            "",
            "Complete reference of all events in the ModESP system.",
            "",
        ]
        
        # Group events by module
        events_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'event_bus' in manifest and 'publishes' in manifest['event_bus']:
                module_name = module.name
                events_by_module[module_name] = manifest['event_bus']['publishes']
        
        # Generate documentation for each module
        for module_name, events in sorted(events_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for event_name, event_info in sorted(events.items()):
                const_name = event_name.upper().replace('.', '_')
                content.append(f"### `{event_name}`")
                content.append(f"**Constant**: `Events::{const_name}`")
                content.append("")
                
                if 'description' in event_info:
                    content.append(f"**Description**: {event_info['description']}")
                    content.append("")
                
                if 'payload' in event_info:
                    content.append("**Payload**:")
                    for field, field_info in event_info['payload'].items():
                        field_type = field_info.get('type', 'unknown')
                        field_desc = field_info.get('description', '')
                        content.append(f"- `{field}` ({field_type}): {field_desc}")
                    content.append("")
                
                content.append("**Example**:")
                content.append("```cpp")
                content.append(f'EventBus::publish(Events::{const_name}, {{')
                if 'payload' in event_info:
                    for field in list(event_info['payload'].keys())[:2]:
                        content.append(f'    {{"{field}", value}},')
                content.append('});')
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_states_doc(self, contract_dir):
        """Generate detailed SharedState documentation"""
        output_file = contract_dir / "states.md"
        
        content = [
            "# SharedState Keys Documentation",
            "",
            "Complete reference of all SharedState keys in the ModESP system.",
            "",
        ]
        
        # Group states by module
        states_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'shared_state' in manifest and 'publishes' in manifest['shared_state']:
                module_name = module.name
                states_by_module[module_name] = manifest['shared_state']['publishes']
        
        # Generate documentation for each module
        for module_name, states in sorted(states_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for state_key, state_info in sorted(states.items()):
                const_name = state_key.upper().replace('.', '_')
                content.append(f"### `{state_key}`")
                content.append(f"**Constant**: `States::{const_name}`")
                content.append("")
                
                content.append(f"**Type**: `{state_info.get('type', 'unknown')}`")
                
                if 'description' in state_info:
                    content.append(f"**Description**: {state_info['description']}")
                
                if 'update_rate_ms' in state_info:
                    content.append(f"**Update Rate**: {state_info['update_rate_ms']}ms")
                
                content.append("")
                content.append("**Example**:")
                content.append("```cpp")
                
                state_type = state_info.get('type', 'auto')
                if state_type == 'float' or state_type == 'number':
                    content.append(f"float value = SharedState::get<float>(States::{const_name});")
                elif state_type == 'boolean':
                    content.append(f"bool value = SharedState::get<bool>(States::{const_name});")
                else:
                    content.append(f"auto value = SharedState::get(States::{const_name});")
                
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_contract_examples(self, contract_dir):
        """Generate example code for common scenarios"""
        examples_dir = contract_dir / "examples"
        examples_dir.mkdir(parents=True, exist_ok=True)
        
        # Temperature monitoring example
        temp_example = examples_dir / "temperature_monitoring.cpp"
        with open(temp_example, 'w', encoding='utf-8') as f:
            f.write('''/**
 * @file temperature_monitoring.cpp
 * @brief Example: Monitoring temperature sensors
 */

#include "generated_system_contract.h"
#include "event_bus.h"
#include "shared_state.h"

class TemperatureMonitor {
public:
    void init() {
        // Subscribe to temperature updates
        EventBus::subscribe(Events::SENSOR_READING_UPDATED, 
            [this](const EventBus::Event& event) {
                auto sensor_role = event.data["sensor_role"].get<std::string>();
                
                if (sensor_role == "temperature_evaporator") {
                    float temp = event.data["value"];
                    onEvaporatorTempChanged(temp);
                }
            });
    }
    
    void checkCurrentTemperature() {
        // Read current temperature from SharedState
        float evap_temp = SharedState::get<float>(States::TEMP_EVAPORATOR);
        float ambient_temp = SharedState::get<float>(States::TEMP_AMBIENT);
        
        ESP_LOGI(TAG, "Evaporator: %.1f°C, Ambient: %.1f°C", 
                 evap_temp, ambient_temp);
    }
    
private:
    void onEvaporatorTempChanged(float temp) {
        if (temp < -30.0f) {
            // Publish warning event
            EventBus::publish(Events::SYSTEM_HEALTH_WARNING, {
                {"module", "temperature_monitor"},
                {"reason", "Evaporator too cold"},
                {"value", temp}
            });
        }
    }
    
    static constexpr const char* TAG = "TempMonitor";
};
''')
        
        self.logger.info(f"Generated contract documentation in {contract_dir}")
    
    def generate_event_registry(self):
        """Generate event registry constants to core/include"""
        # Generate to core/include for better component access
        output_file = self.project_root / "components" / "core" / "include" / "generated_system_contract.h"
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        self.logger.info(f"Generating {output_file}")
        
        code = [
            "// AUTO-GENERATED FILE - DO NOT EDIT",
            f"// Generated by process_manifests.py on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "// For detailed documentation see: components/system_contract/",
            "",
            "#pragma once",
            "",
            "namespace ModESP {",
            "",
            "namespace Events {",
            "    // Event name constants for compile-time checking",
        ]
        
        # Group events by module for better organization
        events_by_module = {}
        for module_name, event_list in self.all_events.items():
            module_events = {}
            # Parse module name from event publishers
            for event in event_list:
                if '.' in event:
                    prefix = event.split('.')[0]
                    if prefix not in events_by_module:
                        events_by_module[prefix] = []
                    events_by_module[prefix].append(event)
        
        # Generate grouped constants
        for event in sorted(self.all_events.keys()):
            const_name = event.upper().replace('.', '_')
            code.append(f'    constexpr const char* {const_name} = "{event}";')
        
        code.extend([
            "} // namespace Events",
            "",
            "namespace States {",
            "    // SharedState key constants",
        ])
        
        # Collect all shared state keys
        all_states = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'shared_state' in manifest and 'publishes' in manifest['shared_state']:
                for state_key in manifest['shared_state']['publishes']:
                    all_states[state_key] = manifest['shared_state']['publishes'][state_key]
        
        # Generate state constants
        for state in sorted(all_states.keys()):
            const_name = state.upper().replace('.', '_')
            code.append(f'    constexpr const char* {const_name} = "{state}";')
        
        code.extend([
            "} // namespace States", 
            "",
            "} // namespace ModESP"
        ])
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(code))
    
    def generate_system_contract_documentation(self):
        """Generate comprehensive system contract documentation"""
        # Create system_contract directory
        contract_dir = self.project_root / "components" / "system_contract"
        contract_dir.mkdir(parents=True, exist_ok=True)
        
        self.logger.info(f"Generating system contract documentation in {contract_dir}")
        
        # Generate main README
        self._generate_contract_readme(contract_dir)
        
        # Generate events documentation
        self._generate_events_doc(contract_dir)
        
        # Generate states documentation  
        self._generate_states_doc(contract_dir)
        
        # Generate examples
        self._generate_contract_examples(contract_dir)
    
    def _generate_contract_readme(self, contract_dir):
        """Generate main README for system contract"""
        output_file = contract_dir / "README.md"
        
        content = [
            "# System Contract Documentation",
            "",
            "This directory contains the complete contract documentation for inter-module communication in ModESP.",
            "",
            "## 📚 Contents",
            "",
            "- **[events.md](events.md)** - All system events with payload structures",
            "- **[states.md](states.md)** - All SharedState keys with types and update rates", 
            "- **[examples/](examples/)** - Usage examples for common scenarios",
            "",
            "## 🎯 Quick Reference",
            "",
            "### Using Events",
            "```cpp",
            '#include "generated_system_contract.h"',
            "",
            "// Subscribe to sensor updates",
            "EventBus::subscribe(Events::SENSOR_READING_UPDATED, handler);",
            "",
            "// Publish error",
            'EventBus::publish(Events::SENSOR_ERROR, {{"message", "Timeout"}});',
            "```",
            "",
            "### Using SharedState",
            "```cpp",
            "// Read temperature",
            "float temp = SharedState::get<float>(States::TEMP_EVAPORATOR);",
            "",
            "// Update state",
            'SharedState::set(States::CLIMATE_MODE, "cooling");',
            "```",
            "",
            f"Generated on: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            f"Total events: {len(self.all_events)}",
            f"Total modules: {len(self.modules)}"
        ]
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_events_doc(self, contract_dir):
        """Generate detailed events documentation"""
        output_file = contract_dir / "events.md"
        
        content = [
            "# System Events Documentation",
            "",
            "Complete reference of all events in the ModESP system.",
            "",
        ]
        
        # Group events by module
        events_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'event_bus' in manifest and 'publishes' in manifest['event_bus']:
                module_name = module.name
                events_by_module[module_name] = manifest['event_bus']['publishes']
        
        # Generate documentation for each module
        for module_name, events in sorted(events_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for event_name, event_info in sorted(events.items()):
                const_name = event_name.upper().replace('.', '_')
                content.append(f"### `{event_name}`")
                content.append(f"**Constant**: `Events::{const_name}`")
                content.append("")
                
                if 'description' in event_info:
                    content.append(f"**Description**: {event_info['description']}")
                    content.append("")
                
                if 'payload' in event_info:
                    content.append("**Payload**:")
                    for field, field_info in event_info['payload'].items():
                        field_type = field_info.get('type', 'unknown')
                        field_desc = field_info.get('description', '')
                        content.append(f"- `{field}` ({field_type}): {field_desc}")
                    content.append("")
                
                content.append("**Example**:")
                content.append("```cpp")
                content.append(f'EventBus::publish(Events::{const_name}, {{')
                if 'payload' in event_info:
                    for field in list(event_info['payload'].keys())[:2]:
                        content.append(f'    {{"{field}", value}},')
                content.append('});')
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_states_doc(self, contract_dir):
        """Generate detailed SharedState documentation"""
        output_file = contract_dir / "states.md"
        
        content = [
            "# SharedState Keys Documentation",
            "",
            "Complete reference of all SharedState keys in the ModESP system.",
            "",
        ]
        
        # Group states by module
        states_by_module = {}
        for module in self.modules:
            manifest = self._module_to_dict(module)
            if 'shared_state' in manifest and 'publishes' in manifest['shared_state']:
                module_name = module.name
                states_by_module[module_name] = manifest['shared_state']['publishes']
        
        # Generate documentation for each module
        for module_name, states in sorted(states_by_module.items()):
            content.append(f"## {module_name}")
            content.append("")
            
            for state_key, state_info in sorted(states.items()):
                const_name = state_key.upper().replace('.', '_')
                content.append(f"### `{state_key}`")
                content.append(f"**Constant**: `States::{const_name}`")
                content.append("")
                
                content.append(f"**Type**: `{state_info.get('type', 'unknown')}`")
                
                if 'description' in state_info:
                    content.append(f"**Description**: {state_info['description']}")
                
                if 'update_rate_ms' in state_info:
                    content.append(f"**Update Rate**: {state_info['update_rate_ms']}ms")
                
                content.append("")
                content.append("**Example**:")
                content.append("```cpp")
                
                state_type = state_info.get('type', 'auto')
                if state_type == 'float' or state_type == 'number':
                    content.append(f"float value = SharedState::get<float>(States::{const_name});")
                elif state_type == 'boolean':
                    content.append(f"bool value = SharedState::get<bool>(States::{const_name});")
                else:
                    content.append(f"auto value = SharedState::get(States::{const_name});")
                
                content.append("```")
                content.append("")
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(content))
    
    def _generate_contract_examples(self, contract_dir):
        """Generate example code for common scenarios"""
        examples_dir = contract_dir / "examples"
        examples_dir.mkdir(parents=True, exist_ok=True)
        
        # Temperature monitoring example
        temp_example = examples_dir / "temperature_monitoring.cpp"
        with open(temp_example, 'w', encoding='utf-8') as f:
            f.write('''/**
 * @file temperature_monitoring.cpp
 * @brief Example: Monitoring temperature sensors
 */

#include "generated_system_contract.h"
#include "event_bus.h"
#include "shared_state.h"

class TemperatureMonitor {
public:
    void init() {
        // Subscribe to temperature updates
        EventBus::subscribe(Events::SENSOR_READING_UPDATED, 
            [this](const EventBus::Event& event) {
                auto sensor_role = event.data["sensor_role"].get<std::string>();
                
                if (sensor_role == "temperature_evaporator") {
                    float temp = event.data["value"];
                    onEvaporatorTempChanged(temp);
                }
            });
    }
    
    void checkCurrentTemperature() {
        // Read current temperature from SharedState
        float evap_temp = SharedState::get<float>(States::TEMP_EVAPORATOR);
        float ambient_temp = SharedState::get<float>(States::TEMP_AMBIENT);
        
        ESP_LOGI(TAG, "Evaporator: %.1f°C, Ambient: %.1f°C", 
                 evap_temp, ambient_temp);
    }
    
private:
    void onEvaporatorTempChanged(float temp) {
        if (temp < -30.0f) {
            // Publish warning event
            EventBus::publish(Events::SYSTEM_HEALTH_WARNING, {
                {"module", "temperature_monitor"},
                {"reason", "Evaporator too cold"},
                {"value", temp}
            });
        }
    }
    
    static constexpr const char* TAG = "TempMonitor";
};
''')
        
        self.logger.info(f"Generated contract documentation in {contract_dir}")


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Process ModESP manifests")
    parser.add_argument(
        '--project-root',
        default='.',
        help='Project root directory (default: current directory)'
    )
    parser.add_argument(
        '--output-dir',
        default='main/generated',
        help='Output directory for generated code (default: main/generated)'
    )
    parser.add_argument(
        '--verbose',
        action='store_true',
        help='Enable verbose logging'
    )
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    processor = ManifestProcessor(args.project_root, args.output_dir)
    processor.process_all()


if __name__ == '__main__':
    main()
