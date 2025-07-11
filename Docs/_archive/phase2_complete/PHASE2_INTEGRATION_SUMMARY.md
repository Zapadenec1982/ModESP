# Phase 2: Runtime Integration Summary

## Overview
We've successfully integrated the manifest system with the ModESP runtime. This document summarizes the changes and provides guidance for future development.

## Key Components Added

### 1. **ManifestReader** (`components/core/manifest_reader.h/cpp`)
- Singleton class that reads generated manifest data
- Provides APIs to query module information, dependencies, and load order
- Validates dependencies and detects circular references
- Integrates with RPC system for auto-registration of APIs

### 2. **ModuleFactory** (`components/core/module_factory.h/cpp`)
- Factory pattern for creating module instances
- Modules register their creation functions using macros
- Enables dynamic module instantiation based on manifest data

### 3. **ModuleManager Updates** (`components/core/module_manager.cpp`)
- Added `register_modules_from_manifests()` function
- Integrated manifest data into module configuration
- Enhanced logging to show manifest information
- Auto-registers generated APIs during RPC setup

## How to Register a Module

### Step 1: Create Module Manifest
Create a `module_manifest.json` in your module's directory:

```json
{
  "name": "MyModule",
  "type": "standard",
  "version": "1.0.0",
  "description": "My custom module",
  "priority": "normal",
  "dependencies": ["SharedState", "EventBus"],
  "config_file": "mymodule.json"
}
```

### Step 2: Register with Factory
Add to your module's .cpp file:

```cpp
#include "module_factory.h"

// At the end of the file
REGISTER_MODULE(MyModule);

// Or with custom name
REGISTER_MODULE_AS(MyModuleImpl, "MyModule");
```

### Step 3: Run Manifest Processor
Generate updated manifest data:

```bash
python C:\ModESP_dev\tools\process_manifests.py \
    --project-root C:\ModESP_dev \
    --output-dir C:\ModESP_dev\main\generated
```

## Integration Flow

1. **System Startup**:
   - ModuleManager::init() initializes ManifestReader
   - ManifestReader loads all generated module info
   - Dependencies are validated and load order determined

2. **Module Registration**:
   - Call ModuleManager::register_modules_from_manifests()
   - For each module in manifest:
     - Check dependencies are satisfied
     - Use ModuleFactory to create instance
     - Register with appropriate priority

3. **Configuration**:
   - ModuleManager uses manifest's config_file field
   - Falls back to naming convention if not specified

4. **RPC Registration**:
   - Generated APIs are auto-registered
   - Module-specific RPCs are registered after

## Example Usage

```cpp
// In main.cpp or system initialization
esp_err_t init_system() {
    // Initialize ModuleManager
    ModuleManager::init();
    
    // Register modules from manifests
    ModuleManager::register_modules_from_manifests();
    
    // Manual registration still supported
    auto customModule = std::make_unique<CustomModule>();
    ModuleManager::register_module(std::move(customModule));
    
    // Load configuration
    auto config = load_config();
    ModuleManager::configure_all(config);
    
    // Initialize all modules
    ModuleManager::init_all();
    
    return ESP_OK;
}
```

## Benefits

1. **Declarative Module Definition**: Modules are defined in JSON manifests
2. **Automatic Dependency Management**: System validates and orders modules
3. **Type Safety**: Generated code provides compile-time checks
4. **Centralized API Registry**: All module APIs in one place
5. **UI Schema Support**: Ready for web interface generation

## Next Steps

### Phase 3: Event System Integration
- Use generated event constants
- Validate event names at compile time
- Auto-generate event documentation

### Phase 4: UI Generation
- Use generated UI schemas
- Create dynamic web interfaces
- Support for module-specific settings

### Future Enhancements
1. **Module Hot-Reload**: Reload modules without restart
2. **Version Checking**: Ensure compatible module versions
3. **Plugin System**: Load modules from external files
4. **Dependency Injection**: Better handling of module dependencies

## Troubleshooting

### Module Not Found
- Ensure manifest is in correct location
- Run manifest processor
- Check module is registered with factory

### Circular Dependencies
- Review module dependencies in manifests
- Use ManifestReader::dumpManifestInfo() for debugging

### Factory Registration Issues
- Ensure REGISTER_MODULE is in .cpp file
- Check module name matches manifest
- Verify factory singleton is initialized

## API Reference

### ModuleManager
```cpp
// Register all modules from manifests
esp_err_t register_modules_from_manifests();
```

### ManifestReader
```cpp
// Get module manifest
auto manifest = ManifestReader::getInstance().getModuleManifest("ModuleName");

// Check dependencies
std::vector<std::string> missing;
manifestReader.validateDependencies("ModuleName", missing);

// Get load order
std::vector<std::string> order;
manifestReader.getModuleLoadOrder(order);
```

### ModuleFactory
```cpp
// Register module
REGISTER_MODULE(MyModule);

// Create module
auto module = ModuleFactory::getInstance().createModule("MyModule");
```
