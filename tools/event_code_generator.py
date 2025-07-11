/**
 * @file event_code_generator.py
 * @brief Generates type-safe event code from module manifests
 * 
 * This script extends process_manifests.py to generate:
 * - Event publisher classes with proper typing
 * - Event subscriber helpers
 * - Documentation for each event
 */

import json
import os
from pathlib import Path
from typing import Dict, List, Any
from datetime import datetime

def to_pascal_case(snake_str: str) -> str:
    """Convert snake_case to PascalCase"""
    return ''.join(word.capitalize() for word in snake_str.split('_'))

def to_camel_case(snake_str: str) -> str:
    """Convert snake_case to camelCase"""
    components = snake_str.split('_')
    return components[0] + ''.join(x.capitalize() for x in components[1:])

def generate_event_publisher(event: Dict[str, Any], module_name: str) -> str:
    """Generate a type-safe publisher method for an event"""
    event_name = event['name']
    method_name = f"publish{to_pascal_case(event_name.replace('.', '_'))}"
    constant_name = event_name.upper().replace('.', '_')
    
    # Build parameter list from payload
    params = []
    param_docs = []
    json_fields = []
    
    if 'payload' in event:
        for field, desc in event['payload'].items():
            # Infer type from description or use string as default
            cpp_type = "const std::string&"
            if 'count' in desc.lower() or 'number' in desc.lower():
                cpp_type = "uint32_t"
            elif 'score' in desc.lower() or 'percent' in desc.lower():
                cpp_type = "uint8_t"
            elif 'time' in desc.lower():
                cpp_type = "uint64_t"
            elif 'value' in desc.lower() or 'temperature' in desc.lower():
                cpp_type = "float"
                
            param_name = to_camel_case(field)
            params.append(f"{cpp_type} {param_name}")
            param_docs.append(f"     * @param {param_name} {desc}")
            json_fields.append(f'        {{"{field}", {param_name}}}')
    
    # Determine priority based on event type
    priority = "EventBus::Priority::NORMAL"
    if 'error' in event_name or 'warning' in event_name:
        priority = "EventBus::Priority::HIGH"
    elif 'critical' in event_name or 'alarm' in event_name:
        priority = "EventBus::Priority::CRITICAL"
    
    method = f"""
    /**
     * @brief Publish {event.get('description', event_name)}
{chr(10).join(param_docs) if param_docs else '     * No parameters'}
     */
    static esp_err_t {method_name}({', '.join(params) if params else 'void'}) {{
        nlohmann::json data = {{
{','.join(json_fields) if json_fields else ''}{"," if json_fields else ""}
            {{"timestamp", esp_timer_get_time()}}
        }};
        return EventBus::publish(Events::{constant_name}, data, {priority});
    }}"""
    
    return method

def generate_event_subscriber(event: Dict[str, Any], module_name: str) -> str:
    """Generate a type-safe subscriber helper for an event"""
    event_name = event['name']
    method_name = f"on{to_pascal_case(event_name.replace('.', '_'))}"
    constant_name = event_name.upper().replace('.', '_')
    
    # Build handler signature from payload
    handler_params = []
    param_extracts = []
    
    if 'payload' in event:
        for field, desc in event['payload'].items():
            # Infer type
            cpp_type = "const std::string&"
            default_value = '""'
            extract_type = "std::string"
            
            if 'count' in desc.lower() or 'number' in desc.lower():
                cpp_type = "uint32_t"
                default_value = "0"
                extract_type = "uint32_t"
            elif 'score' in desc.lower() or 'percent' in desc.lower():
                cpp_type = "uint8_t"
                default_value = "0"
                extract_type = "uint8_t"
            elif 'time' in desc.lower():
                cpp_type = "uint64_t"
                default_value = "0"
                extract_type = "uint64_t"
            elif 'value' in desc.lower() or 'temperature' in desc.lower():
                cpp_type = "float"
                default_value = "0.0f"
                extract_type = "float"
                
            param_name = to_camel_case(field)
            handler_params.append(f"{cpp_type} {param_name}")
            param_extracts.append(
                f'                auto {param_name} = event.data.value("{field}", {extract_type}({default_value}));'
            )
    
    handler_signature = f"std::function<void({', '.join(handler_params) if handler_params else 'void'})>"
    
    method = f"""
    /**
     * @brief Subscribe to {event.get('description', event_name)}
     */
    static EventBus::SubscriptionHandle {method_name}(
        {handler_signature} handler) {{
        
        return EventBus::subscribe(Events::{constant_name},
            [handler](const EventBus::Event& event) {{
{chr(10).join(param_extracts) if param_extracts else ''}
                handler({', '.join(to_camel_case(field) for field in event.get('payload', {}).keys())});
            }});
    }}"""
    
    return method

def generate_event_helpers(manifest_dir: Path, output_path: Path):
    """Generate complete event helper classes from manifests"""
    
    events_by_module = {}
    all_events = []
    
    # Collect all events from manifests
    for manifest_file in manifest_dir.rglob("module_manifest.json"):
        with open(manifest_file, 'r') as f:
            manifest = json.load(f)
            
        module_name = manifest.get('name', 'Unknown')
        events = manifest.get('events', [])
        
        if events:
            events_by_module[module_name] = events
            all_events.extend(events)
    
    # Generate header file
    header = f"""/**
 * @file generated_event_helpers.h
 * @brief Auto-generated type-safe event helpers
 * 
 * Generated by event_code_generator.py on {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
 * DO NOT EDIT MANUALLY
 */

#pragma once

#include "event_bus.h"
#include "generated_events.h"
#include "nlohmann/json.hpp"
#include <esp_timer.h>

namespace ModESP {{

/**
 * @brief Auto-generated event publishers
 */
class GeneratedEventPublisher {{
public:"""
    
    # Generate publisher methods
    for module, events in events_by_module.items():
        header += f"\n    // Events from {module}"
        for event in events:
            header += generate_event_publisher(event, module)
    
    header += "\n};\n\n"
    
    # Generate subscriber helpers
    header += """/**
 * @brief Auto-generated event subscribers
 */
class GeneratedEventSubscriber {
public:"""
    
    for module, events in events_by_module.items():
        header += f"\n    // Events from {module}"
        for event in events:
            header += generate_event_subscriber(event, module)
    
    header += "\n};\n\n} // namespace ModESP\n"
    
    # Write output
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w') as f:
        f.write(header)
    
    print(f"Generated event helpers: {output_path}")
    print(f"  - {len(all_events)} total events")
    print(f"  - {len(events_by_module)} modules with events")

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Generate type-safe event helpers")
    parser.add_argument("--manifest-dir", required=True, help="Root directory containing manifests")
    parser.add_argument("--output", required=True, help="Output file path")
    
    args = parser.parse_args()
    
    generate_event_helpers(Path(args.manifest_dir), Path(args.output))
