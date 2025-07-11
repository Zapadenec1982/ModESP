#!/usr/bin/env python3
"""
Generate comprehensive system contract documentation from manifests
"""

def generate_system_contract(manifests, output_dir):
    """Generate both code constants and developer documentation"""
    
    # 1. Generate constants header (for compile-time safety)
    with open(output_dir / "generated_system_contract.h", "w") as f:
        f.write("""
/**
 * @file generated_system_contract.h
 * @brief System-wide events and states contract
 * 
 * AUTO-GENERATED - DO NOT EDIT
 * 
 * This file contains all events and SharedState keys used in the system.
 * Use these constants instead of string literals for compile-time safety.
 * 
 * For detailed documentation, see generated_system_contract.md
 */

#pragma once

namespace ModESP {

namespace Events {
""")
        
        # Group events by module
        events_by_module = {}
        for module in manifests:
            if 'event_bus' in module and 'publishes' in module['event_bus']:
                module_name = module['module']['name']
                events_by_module[module_name] = module['event_bus']['publishes']
        
        # Generate constants with comments
        for module_name, events in events_by_module.items():
            f.write(f"\n    // === {module_name} Events ===\n")
            for event_name, event_info in events.items():
                const_name = event_name.upper().replace('.', '_')
                f.write(f"    constexpr const char* {const_name} = \"{event_name}\";\n")
        
        f.write("\n} // namespace Events\n\n")
        
        # Similar for SharedState keys
        f.write("namespace States {\n")
        
        states_by_module = {}
        for module in manifests:
            if 'shared_state' in module and 'publishes' in module['shared_state']:
                module_name = module['module']['name']
                states_by_module[module_name] = module['shared_state']['publishes']
        
        for module_name, states in states_by_module.items():
            f.write(f"\n    // === {module_name} States ===\n")
            for state_key, state_info in states.items():
                const_name = state_key.upper().replace('.', '_')
                f.write(f"    constexpr const char* {const_name} = \"{state_key}\";\n")
        
        f.write("\n} // namespace States\n\n} // namespace ModESP\n")
    
    # 2. Generate detailed documentation (for developers)
    with open(output_dir / "generated_system_contract.md", "w") as f:
        f.write("""# System Contract Documentation

This document describes all events and shared states in the ModESP system.
Use this as a reference when developing new modules.

## 📡 Events

""")
        
        # Detailed event documentation
        for module_name, events in events_by_module.items():
            f.write(f"### {module_name}\n\n")
            for event_name, event_info in events.items():
                f.write(f"#### `{event_name}`\n")
                f.write(f"- **Description**: {event_info.get('description', 'N/A')}\n")
                f.write(f"- **Constant**: `Events::{event_name.upper().replace('.', '_')}`\n")
                
                if 'payload' in event_info:
                    f.write("- **Payload**:\n")
                    for field, field_info in event_info['payload'].items():
                        f.write(f"  - `{field}`: {field_info.get('type', 'unknown')} - {field_info.get('description', '')}\n")
                
                f.write("\n**Example usage**:\n")
                f.write("```cpp\n")
                f.write(f"// Publishing\n")
                f.write(f"EventBus::publish(Events::{event_name.upper().replace('.', '_')}, data);\n")
                f.write(f"\n// Subscribing\n")
                f.write(f"EventBus::subscribe(Events::{event_name.upper().replace('.', '_')}, handler);\n")
                f.write("```\n\n")
        
        # Similar for SharedState
        f.write("\n## 🗄️ Shared States\n\n")
        
        for module_name, states in states_by_module.items():
            f.write(f"### {module_name}\n\n")
            for state_key, state_info in states.items():
                f.write(f"#### `{state_key}`\n")
                f.write(f"- **Type**: {state_info.get('type', 'unknown')}\n")
                f.write(f"- **Description**: {state_info.get('description', 'N/A')}\n")
                f.write(f"- **Constant**: `States::{state_key.upper().replace('.', '_')}`\n")
                f.write(f"- **Update rate**: {state_info.get('update_rate_ms', 'N/A')}ms\n")
                
                f.write("\n**Example usage**:\n")
                f.write("```cpp\n")
                f.write(f"// Reading\n")
                f.write(f"auto value = SharedState::get<float>(States::{state_key.upper().replace('.', '_')});\n")
                f.write(f"\n// Writing\n")
                f.write(f"SharedState::set(States::{state_key.upper().replace('.', '_')}, 23.5f);\n")
                f.write("```\n\n")
    
    # 3. Generate Visual Studio Code snippets (bonus!)
    generate_vscode_snippets(events_by_module, states_by_module, output_dir)
