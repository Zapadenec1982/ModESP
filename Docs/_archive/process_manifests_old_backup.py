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
        
        # Step 3: Resolve dependencies
        self.resolve_dependencies()
        
        # Step 4: Generate code
        self.generate_code()
        
        self.logger.info("Manifest processing complete!")
    
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
            manifest_path=str(manifest_path)
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
        self.generate_ui_schemas()
        self.generate_event_registry()
        
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
            code.extend([
                "    {",
                f'        .name = "{module.name}",',
                f'        .type = ModuleType::{module.type},',
                f'        .version = "{module.version}",',
                f'        .description = "{module.description}",',
                f'        .priority = Priority::{module.priority},',
                f'        .dependencies = {{' + ', '.join(f'"{dep}"' for dep in module.dependencies) + '},',
                config_value = f'"{module.config_file}"' if module.config_file else 'nullptr'
                code.append(f'        .config_file = {config_value},'),
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
        ]
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(code))
    
    def generate_event_registry(self):
        """Generate event registry for compile-time checking"""
        output_file = self.output_dir / "generated_events.h"
        
        self.logger.info(f"Generating {output_file}")
        
        code = [
            "// AUTO-GENERATED FILE - DO NOT EDIT",
            f"// Generated by process_manifests.py on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
            "",
            "#pragma once",
            "",
            "namespace ModESP {",
            "namespace Events {",
            "",
            "// Event name constants for compile-time checking",
        ]
        
        for event in sorted(self.all_events.keys()):
            const_name = event.upper().replace('.', '_')
            code.append(f'constexpr const char* {const_name} = "{event}";')
        
        code.extend([
            "",
            "} // namespace Events", 
            "} // namespace ModESP"
        ])
        
        with open(output_file, 'w', encoding='utf-8') as f:
            f.write('\n'.join(code))


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
