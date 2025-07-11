# Generated Events Include Problem

## Problem Description

The `core` component tries to include `generated_events.h` in:
- `event_helpers.h`
- `event_validator.cpp`

However, the file is generated in `main/generated/` by `process_manifests.py`.

## ✅ RESOLVED!

The issue was that build scripts didn't run code generation before compilation.

### Fix Applied:

Updated both `build.bat` and `build.ps1` to run:
```bash
python tools\process_manifests.py --project-root . --output-dir main\generated
```

This ensures all generated files (including `generated_events.h`) are created before the build starts.

### How it works now:

1. **Build script runs** → Generates all files in `main/generated/`
2. **Core component** → Can include files because build system knows about paths
3. **Compilation succeeds** → Everything works!

## Build Process

Current build flow:
1. Run `build.bat` or `build.ps1`
2. Script generates code from manifests
3. Script runs `idf.py build`
4. All components compile successfully

## Notes

- The `main` component's CMakeLists.txt doesn't explicitly add `generated/` to INCLUDE_DIRS
- This might work because ESP-IDF automatically includes subdirectories
- Or there might be global include paths set elsewhere
- Current solution works without modifying CMakeLists files
