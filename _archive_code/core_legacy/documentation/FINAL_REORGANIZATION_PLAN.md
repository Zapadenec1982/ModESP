# Core Component Reorganization Plan

## Current situation after memory pool removal:

### Files in core (23 files):
1. **Application** (3 files)
   - application.cpp/h
   - application_wrapper.cpp

2. **Module Manager** (8 files!)
   - module_manager.cpp/h
   - module_registry.cpp/h
   - module_factory.cpp/h
   - module_heartbeat.cpp/h
   - manifest_reader.cpp/h (build-time tool?)

3. **Config Manager** (4 files)
   - config_manager.cpp/h
   - config_manager_async.cpp/h

4. **Event Bus** (3 files)
   - event_bus.cpp/h
   - event_helpers.h

5. **Shared State** (2 files)
   - shared_state.cpp/h

6. **Utilities** (3 files)
   - diagnostic_tools.cpp/h
   - error_handling.h

## Recommendations:

### Option 1: Keep flat structure but consolidate Module Manager
- Merge module_registry + module_factory → module_manager.cpp
- Keep module_heartbeat separate (optional feature)
- Move manifest_reader to tools/ or keep if needed at runtime

### Option 2: Organize by subsystem (folders)
```
core/
├── application/
├── modules/      # All module-related files
├── config/
├── events/
├── state/
└── utils/
```

### Option 3: Move utilities out
- diagnostic_tools → core_utils component
- error_handling.h → core_utils component

What's your preference?
1. Flat structure with file consolidation?
2. Folder organization?
3. Both - consolidate AND organize?
