# System Contract Restructuring

## Changes Made

### 1. Moved Constants Generation
- **From**: `main/generated/generated_events.h`
- **To**: `components/core/include/generated_system_contract.h`
- **Reason**: Better access for core component, no cross-component dependencies

### 2. Created Documentation Component
- **Location**: `components/system_contract/`
- **Contents**:
  - `README.md` - Overview and quick reference
  - `events.md` - Detailed events documentation
  - `states.md` - SharedState keys documentation
  - `examples/` - Usage examples
  - `CMakeLists.txt` - Empty component registration

### 3. Updated File References
- `event_helpers.h` - Now includes `generated_system_contract.h`
- `event_validator.cpp` - Now includes `generated_system_contract.h`

### 4. Updated process_manifests.py
- Generates constants to `core/include/`
- Generates documentation to `system_contract/`
- Added SharedState constants generation

### 5. Updated Build Scripts
- `build.bat` - Runs manifest processing before build
- `build.ps1` - Runs manifest processing before build

## New Structure

```
components/
├── core/
│   ├── include/
│   │   ├── generated_system_contract.h  # Constants only
│   │   ├── json_rpc_interface.h
│   │   └── system_contract.h
│   └── ...
├── system_contract/                      # Documentation only
│   ├── README.md
│   ├── events.md
│   ├── states.md
│   ├── examples/
│   └── CMakeLists.txt
└── ...

main/generated/                           # Code only, no docs
├── generated_api_registry.cpp
├── generated_module_info.cpp
└── ...
```

## Benefits

1. **Clear Separation**: Code vs Documentation
2. **Better Organization**: Developers know where to find contract info
3. **No Runtime Overhead**: Documentation doesn't affect binary
4. **IDE Support**: system_contract appears as component in IDE
5. **Compile-time Safety**: Constants in core/include/

## For Developers

When developing a new module:
1. Check `components/system_contract/` for available events/states
2. Use constants from `#include "generated_system_contract.h"`
3. See examples in `components/system_contract/examples/`

## Next Steps

1. Remove EventValidator (runtime validation) - not needed
2. Consider adding VSCode snippets generation
3. Add more examples to system_contract/examples/
