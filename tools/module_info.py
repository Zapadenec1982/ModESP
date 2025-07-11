#!/usr/bin/env python3
"""
Simple Module Information Tool
Reads module descriptors and shows API/integration info for developers
"""

import json
from pathlib import Path
import argparse

def find_module_descriptors(components_dir='components'):
    """Find all module descriptor files"""
    descriptors = {}
    components_path = Path(components_dir)
    
    # Look for both new descriptors and legacy ui_schema files
    for desc_file in components_path.glob("*/module_descriptor.json"):
        module_name = desc_file.parent.name
        with open(desc_file, 'r') as f:
            descriptors[module_name] = json.load(f)
            
    for ui_file in components_path.glob("*/ui_schema.json"):
        module_name = ui_file.parent.name
        if module_name not in descriptors:  # Don't override full descriptors
            with open(ui_file, 'r') as f:
                ui_data = json.load(f)
                descriptors[module_name] = {"ui_schema": ui_data, "legacy": True}
                
    return descriptors

def show_module_list(descriptors):
    """Show available modules"""
    print("📦 Available Modules:")
    for name, desc in descriptors.items():
        if desc.get('legacy'):
            print(f"  • {name} (legacy ui_schema only)")
        else:
            info = desc.get('module_info', {})
            enabled = "✅" if info.get('enabled', True) else "❌"
            print(f"  • {name} {enabled} - {info.get('description', 'No description')}")

def show_module_api(module_name, descriptor):
    """Show detailed module API information"""
    print(f"\n🔌 {module_name} API Reference")
    print("="*50)
    
    if descriptor.get('legacy'):
        print("⚠️  Legacy module - only UI schema available")
        return
        
    # Module info
    info = descriptor.get('module_info', {})
    print(f"📋 Module: {info.get('name', module_name)} v{info.get('version', '?')}")
    print(f"📝 Description: {info.get('description', 'No description')}")
    print(f"⚡ Priority: {info.get('priority', 'STANDARD')}")
    print(f"🔄 Enabled: {'Yes' if info.get('enabled', True) else 'No'}")
    
    # API Reference
    api = descriptor.get('api_reference', {})
    
    # RPC Methods
    rpc_methods = api.get('rpc_methods', {})
    if rpc_methods:
        print(f"\n🔧 RPC Methods ({len(rpc_methods)}):")
        for method, details in rpc_methods.items():
            print(f"  📞 {method}")
            print(f"     {details.get('description', 'No description')}")
            if 'example' in details:
                print(f"     💡 Example: {details['example']}")
                
    # SharedState Keys
    state_keys = api.get('shared_state_keys', {})
    if state_keys:
        print(f"\n🗂️  SharedState Keys ({len(state_keys)}):")
        for key, details in state_keys.items():
            print(f"  🔑 {key} ({details.get('type', 'unknown')})")
            print(f"     {details.get('description', 'No description')}")
            print(f"     🔄 Updates: {details.get('update_frequency', 'unknown')}")
            
    # Events
    events_pub = api.get('events_published', {})
    if events_pub:
        print(f"\n📢 Events Published ({len(events_pub)}):")
        for event, details in events_pub.items():
            print(f"  📡 {event}")
            print(f"     {details.get('description', 'No description')}")
            
    events_sub = api.get('events_subscribed', {})
    if events_sub:
        print(f"\n📥 Events Subscribed ({len(events_sub)}):")
        for event, description in events_sub.items():
            print(f"  📨 {event} - {description}")

def show_integration_guide(module_name, descriptor):
    """Show how to integrate with this module"""
    print(f"\n🤝 How to integrate with {module_name}")
    print("="*50)
    
    guide = descriptor.get('integration_guide', {})
    
    # Dependencies
    deps = guide.get('dependencies', [])
    if deps:
        print(f"📦 Dependencies: {', '.join(deps)}")
        
    # Config file
    config_file = guide.get('config_file')
    if config_file:
        print(f"⚙️  Config file: {config_file}")
        
    # How to interact
    interactions = guide.get('how_to_interact', {})
    from_modules = interactions.get('from_other_modules', {})
    if from_modules:
        print(f"\n💻 Code Examples:")
        for action, code in from_modules.items():
            print(f"  • {action}:")
            print(f"    {code}")
            
    # For new developers
    for_devs = interactions.get('for_new_developers', {})
    if for_devs:
        print(f"\n👨‍💻 For New Developers:")
        print(f"  📖 Understanding: {for_devs.get('understanding', 'N/A')}")
        key_files = for_devs.get('key_files', [])
        if key_files:
            print(f"  📁 Key files: {', '.join(key_files)}")
        testing = for_devs.get('testing')
        if testing:
            print(f"  🧪 Testing: {testing}")

def enable_disable_module(module_name, enable, components_dir='components'):
    """Enable or disable a module"""
    desc_file = Path(components_dir) / module_name / "module_descriptor.json"
    
    if not desc_file.exists():
        print(f"❌ Module descriptor not found: {desc_file}")
        return False
        
    try:
        with open(desc_file, 'r') as f:
            descriptor = json.load(f)
            
        descriptor.setdefault('module_info', {})['enabled'] = enable
        
        with open(desc_file, 'w') as f:
            json.dump(descriptor, f, indent=4)
            
        status = "enabled" if enable else "disabled"
        print(f"✅ Module {module_name} {status}")
        return True
        
    except Exception as e:
        print(f"❌ Error updating module: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description='ModESP Module Information Tool')
    parser.add_argument('--components', default='components', help='Components directory')
    parser.add_argument('--list', action='store_true', help='List all modules')
    parser.add_argument('--api', help='Show API for specific module')
    parser.add_argument('--guide', help='Show integration guide for module')
    parser.add_argument('--enable', help='Enable specific module')
    parser.add_argument('--disable', help='Disable specific module')
    
    args = parser.parse_args()
    
    descriptors = find_module_descriptors(args.components)
    
    if args.list or (not args.api and not args.guide and not args.enable and not args.disable):
        show_module_list(descriptors)
        
    if args.api:
        if args.api in descriptors:
            show_module_api(args.api, descriptors[args.api])
        else:
            print(f"❌ Module '{args.api}' not found")
            
    if args.guide:
        if args.guide in descriptors:
            show_integration_guide(args.guide, descriptors[args.guide])
        else:
            print(f"❌ Module '{args.guide}' not found")
            
    if args.enable:
        enable_disable_module(args.enable, True, args.components)
        
    if args.disable:
        enable_disable_module(args.disable, False, args.components)

if __name__ == '__main__':
    main() 