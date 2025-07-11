# adaptive_ui_generator.py
# Extension for process_manifests.py to generate Phase 5 components

import json
from typing import List, Dict, Any
from pathlib import Path

class AdaptiveUIGenerator:
    """Generator for Phase 5 Adaptive UI components"""
    
    def __init__(self, modules: List[Dict], drivers: List[Dict]):
        self.modules = modules
        self.drivers = drivers
        self.all_components = []
        
    def generate_all_components(self) -> None:
        """Main generation method"""
        # 1. Collect all UI components
        self._collect_components()
        
        # 2. Generate C++ headers
        self._generate_component_registry()
        self._generate_component_metadata()
        self._generate_factory_functions()
        
    def _collect_components(self) -> None:
        """Collect all possible UI components from manifests"""
        # From manager modules
        for module in self.modules:
            if module.get('module', {}).get('type') == 'MANAGER':
                ui_adaptive = module.get('ui', {}).get('adaptive', {})
                components = ui_adaptive.get('components', [])
                for comp in components:
                    comp['source'] = module['module']['name']
                    self.all_components.append(comp)
        
        # From driver extensions
        for driver in self.drivers:
            ui_ext = driver.get('ui_extensions', {})
            components = ui_ext.get('components', [])
            for comp in components:
                comp['source'] = driver['driver']['name']
                self.all_components.append(comp)
                
    def _generate_component_registry(self) -> str:
        """Generate component registry header"""
        output = """// generated_ui_components.h
// AUTO-GENERATED - DO NOT EDIT
#pragma once

#include "ui_component_base.h"
#include <array>

namespace ModESP::UI {

// Component metadata
struct ComponentInfo {
    const char* id;
    ComponentType type;
    const char* condition;
    AccessLevel min_access;
    Priority priority;
    bool lazy_loadable;
    const char* source;
};

// All possible components
constexpr ComponentInfo ALL_COMPONENTS[] = {
"""
        
        for comp in self.all_components:
            output += f"""    {{
        "{comp['id']}",
        ComponentType::{comp['type'].upper()},
        "{comp.get('condition', 'always')}",
        AccessLevel::{comp.get('access_level', 'user').upper()},
        Priority::{comp.get('priority', 'medium').upper()},
        {str(comp.get('lazy_load', True)).lower()},
        "{comp['source']}"
    }},
"""
        
        output += f"""
}};

constexpr size_t COMPONENT_COUNT = {len(self.all_components)};

}} // namespace ModESP::UI
"""
        return output
    
    def _generate_component_metadata(self) -> str:
        """Generate component metadata"""
        # Implementation here
        pass
        
    def _generate_factory_functions(self) -> str:
        """Generate factory functions for components"""
        output = """// generated_component_factories.cpp
// AUTO-GENERATED - DO NOT EDIT

#include "lazy_component_loader.h"
#include "ui_component_base.h"

namespace ModESP::UI {

void registerAllComponentFactories(LazyComponentLoader& loader) {
"""
        
        for comp in self.all_components:
            output += f"""
    // {comp['id']} from {comp['source']}
    loader.registerComponentFactory("{comp['id']}", []() {{
        return std::make_unique<{self._get_component_class(comp)}>(
            "{comp['id']}", 
            "{comp.get('label', comp['id'])}"
        );
    }});
"""
        
        output += """
}

} // namespace ModESP::UI
"""
        return output
    
    def _get_component_class(self, comp: Dict) -> str:
        """Get C++ class name for component type"""
        type_map = {
            'text': 'TextComponent',
            'slider': 'SliderComponent',
            'dropdown': 'DropdownComponent',
            'button': 'ButtonComponent',
            'toggle': 'ToggleComponent',
            'chart': 'ChartComponent'
        }
        return type_map.get(comp['type'], 'TextComponent')

# Integration function for process_manifests.py
def generate_adaptive_ui(modules: List[Dict], drivers: List[Dict], output_dir: Path):
    """Main entry point for adaptive UI generation"""
    generator = AdaptiveUIGenerator(modules, drivers)
    generator.generate_all_components()
    
    # Write generated files
    registry = generator._generate_component_registry()
    (output_dir / 'generated_ui_components.h').write_text(registry)
    
    factories = generator._generate_factory_functions()
    (output_dir / 'generated_component_factories.cpp').write_text(factories)
    
    print(f"Generated {len(generator.all_components)} UI components")
