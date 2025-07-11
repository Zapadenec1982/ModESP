# Core Component Reorganization Proposal

## Current Issues
- 28 files in flat structure - chaos!
- Module Manager scattered across 8 files
- Memory Pool mixed with core systems
- Manifest Reader doesn't belong in runtime core
- Archive folder still present

## Option 1: Organize by Subsystem (Recommended)

```
components/core/
├── src/
│   ├── application/
│   │   ├── application.cpp
│   │   ├── application.h
│   │   └── application_wrapper.cpp
│   ├── module_manager/
│   │   ├── module_manager.cpp
│   │   ├── module_manager.h
│   │   ├── module_registry.cpp
│   │   ├── module_registry.h
│   │   ├── module_factory.cpp
│   │   ├── module_factory.h
│   │   ├── module_heartbeat.cpp
│   │   └── module_heartbeat.h
│   ├── config_manager/
│   │   ├── config_manager.cpp
│   │   ├── config_manager.h
│   │   ├── config_manager_async.cpp
│   │   └── config_manager_async.h
│   ├── event_bus/
│   │   ├── event_bus.cpp
│   │   ├── event_bus.h
│   │   └── event_helpers.h
│   └── shared_state/
│       ├── shared_state.cpp
│       └── shared_state.h
├── include/            # Public interfaces
│   ├── core/          # Core public headers
│   ├── generated_system_contract.h
│   ├── json_rpc_interface.h
│   └── system_contract.h
├── configs/           # JSON configurations
└── CMakeLists.txt
```

Move to separate components:
- `memory_pool/` → New component
- `manifest_reader/` → New component or to tools/
- `utils/` → New core_utils component

## Option 2: Consolidate Module Files

Merge module files:
- `module_registry.cpp` → into `module_manager.cpp`
- `module_factory.cpp` → into `module_manager.cpp`
- Keep `module_heartbeat` separate (it's optional)

Result: 4 files instead of 8 for Module Manager

## Option 3: Move Non-Core Out

Core should only have the 5 key systems.

Move out:
1. **memory_pool** → Separate component (general utility)
2. **manifest_reader** → Part of build tools, not runtime
3. **diagnostic_tools** → core_utils component
4. **schemas/** → Move to tools/ or docs/

## Recommendations

1. **Use Option 1** - Clear organization by subsystem
2. **Move memory_pool out** - It's a general utility
3. **Move manifest_reader out** - It's build-time, not runtime
4. **Archive _archive_config/** - Already archived elsewhere
5. **Consider moving configs/** - Keep configs near modules that use them

## Benefits
- Clear structure - know where to find things
- Easier navigation - grouped by function
- Better for new developers - obvious organization
- Cleaner CMakeLists.txt - can use GLOB for subdirs
