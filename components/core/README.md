# Core Component - ModESP

## 📌 Overview

The Core component is the minimal coordination kernel for ModESP system (<5KB RAM). It provides essential services for module lifecycle management, event-driven communication, and system configuration.

## 🏗️ Architecture

### Key Components:

1. **Application Controller** (`application.cpp/h`)
   - Main lifecycle coordinator
   - 100Hz main loop
   - System health monitoring
   - Emergency mode handling
   - C wrapper: `application_wrapper.cpp` for main.c integration

2. **SharedState** (`shared_state.cpp/h`)
   - Thread-safe JSON key-value store (2.5-3.5KB RAM)
   - Publish-subscribe mechanism
   - Real-time state synchronization

3. **EventBus** (`event_bus.cpp/h`)
   - Async publish-subscribe messaging (600-800B RAM)
   - Priority-based event handling
   - Helper utilities in `event_helpers.h`

4. **Config Manager** (`config_manager.cpp/h` + `config_manager_async.cpp/h`)
   - JSON-based persistent configuration
   - Lifecycle-aware config management
   - Both sync and async APIs

5. **Module System**
   - **ModuleManager** (`module_manager.cpp/h`) - Lifecycle management
   - **ModuleRegistry** (`module_registry.cpp/h`) - Module registration
   - **ModuleFactory** (`module_factory.cpp/h`) - Module instantiation
   - **ModuleHeartbeat** (`module_heartbeat.cpp/h`) - Health monitoring
   - **ManifestReader** (`manifest_reader.cpp/h`) - JSON manifest parsing

6. **Utilities**
   - **DiagnosticTools** (`diagnostic_tools.cpp/h`) - System diagnostics
   - **ErrorHandling** (`error_handling.h`) - Result<T> and error codes

## 💡 Heap Fragmentation Monitoring

Instead of memory pool, we monitor heap fragmentation:
```cpp
size_t largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
if (free_heap > 20KB && largest_block < 10KB) {
    // Fragmentation detected!
}
```

## � Directory Structure

```
components/core/
├── configs/              # JSON configuration files
├── include/              # Public interfaces
│   ├── json_rpc_interface.h
│   └── system_contract.h
├── schemas/              # JSON validation schemas
├── src/                  # (Currently empty)
├── *.cpp/h              # Core implementation files
└── CMakeLists.txt       # Build configuration
```

## 🔄 Lifecycle Phases

1. **BOOT** - Synchronous initialization (FS → Config → Modules)
2. **RUNTIME** - Asynchronous operation (100Hz loop, auto-save)
3. **SHUTDOWN** - Synchronous cleanup (flush → cleanup)

## 🚧 TODO Items

1. **CPU Usage Fix Integration**
   - File `application_cpu_fix.cpp` contains improved CPU measurement
   - Needs integration into main `application.cpp`
   - See: `_archive_code/core_legacy/cpu_fix_to_integrate/`
 
 
2. **Module Manifests**
   - Manifest files should be co-located with modules
   - Consider manifest organization strategy

## 🗄️ Recent Changes

### 2025-07-09 Reorganization:
- ✅ Organized code by subsystem in `src/` folder
- ✅ Consolidated module_registry + module_factory → module_lifecycle
- ✅ Moved utilities to `utils/` folder
- ✅ Removed memory pool (using heap monitoring instead)
- ✅ Removed EventValidator (compile-time safety only)
- ✅ Cleaned up directory structure

## 🛠️ Build Notes

- Component uses ESP-IDF build system
- Embedded JSON configs from `configs/` directory
- Dependencies: esp_timer, nvs_flash, nlohmann-json, FreeRTOS, etc.
- **Important**: Build scripts (build.bat/build.ps1) run `process_manifests.py` to generate required files
- Generated files (like `generated_events.h`) are created in `main/generated/`
