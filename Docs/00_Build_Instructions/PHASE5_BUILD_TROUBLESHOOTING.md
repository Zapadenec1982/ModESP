# 🔧 Phase 5 Compilation Troubleshooting

## Common Build Errors and Solutions

### 1. Component 'adaptive_ui' not found

**Error:**
```
CMake Error at CMakeLists.txt:123:
  Component adaptive_ui not found
```

**Solution:**
1. Check that `components/adaptive_ui/CMakeLists.txt` exists
2. Verify CMakeLists.txt content:
```cmake
idf_component_register(
    SRCS 
        "ui_filter.cpp"
        "lazy_component_loader.cpp"
    INCLUDE_DIRS "." "include"
    REQUIRES 
        base_module
        mittelab__nlohmann-json
)
```

### 2. ui_component_base.h not found

**Error:**
```
fatal error: ui_component_base.h: No such file or directory
```

**Solution:**
1. Check file exists: `components/adaptive_ui/include/ui_component_base.h`
2. Update include path in CMakeLists.txt:
```cmake
INCLUDE_DIRS "." "include"
```

### 3. Undefined reference to UIComponent methods

**Error:**
```
undefined reference to `ModESP::UI::TextComponent::renderLCD(LCDRenderer&)'
```

**Solution:**
Add stub implementations to ui_component_base.h:
```cpp
class TextComponent : public UIComponent {
public:
    // ... existing code ...
    
    void renderLCD(LCDRenderer& renderer) override {
        // TODO: Implement
    }
    
    void renderWeb(WebRenderer& renderer) override {
        // TODO: Implement
    }
    
    void renderMQTT(MQTTRenderer& renderer) override {
        // TODO: Implement
    }
};
```

### 4. LazyComponentLoader undefined

**Error:**
```
undefined reference to `ModESP::UI::LazyComponentLoader::getInstance()'
```

**Solution:**
Ensure static member is defined in lazy_component_loader.cpp:
```cpp
namespace ModESP::UI {
    LazyComponentLoader* LazyLoaderManager::instance = nullptr;
}
```

### 5. Multiple definition errors

**Error:**
```
multiple definition of `ModESP::UI::ALL_COMPONENTS'
```

**Solution:**
1. Make sure generated files are included only once
2. Use header guards in all .h files:
```cpp
#pragma once
// or
#ifndef FILE_NAME_H
#define FILE_NAME_H
// ... content ...
#endif
```

### 6. JSON parse errors

**Error:**
```
nlohmann::json::parse_error
```

**Solution:**
1. Check all manifest JSON files are valid
2. Validate with: https://jsonlint.com/
3. Common issues:
   - Missing commas
   - Extra commas at end
   - Unclosed brackets

### 7. Component CMakeLists not processed

**Error:**
```
Component adaptive_ui has no source files
```

**Solution:**
Add to main CMakeLists.txt:
```cmake
set(EXTRA_COMPONENT_DIRS 
    "components/adaptive_ui"
)
```

### 8. Missing renderer classes

**Error:**
```
error: 'LCDRenderer' has not been declared
```

**Solution:**
Create renderer stubs in `components/adaptive_ui/include/renderers.h`:
```cpp
#pragma once

namespace ModESP::UI {

class LCDRenderer {
public:
    // Add methods as needed
};

class WebRenderer {
public:
    // Add methods as needed
};

class MQTTRenderer {
public:
    // Add methods as needed
};

class TelegramRenderer {
public:
    // Add methods as needed
};

} // namespace ModESP::UI
```

### 9. ButtonComponent not defined

**Error:**
```
error: 'ButtonComponent' was not declared
```

**Solution:**
Add to ui_component_base.h:
```cpp
class ButtonComponent : public UIComponent {
public:
    ButtonComponent(const std::string& id, const std::string& label)
        : UIComponent(id, label) {}
        
    ComponentType getType() const override { return ComponentType::BUTTON; }
    size_t getEstimatedSize() const override { return 200; }
    
    void renderLCD(LCDRenderer& renderer) override {}
    void renderWeb(WebRenderer& renderer) override {}
    void renderMQTT(MQTTRenderer& renderer) override {}
};
```

### 10. Generated files not updating

**Solution:**
1. Delete generated files:
```cmd
del main\generated\generated_ui_components.h
del main\generated\generated_component_factories.cpp
```

2. Regenerate:
```cmd
python tools\process_manifests.py --project-root . --output-dir main\generated
```

3. Clean build:
```cmd
idf.py fullclean
idf.py build
```

## Quick Fix Script

Create `fix_build.bat`:
```batch
@echo off
echo Fixing Phase 5 build issues...

REM Clean generated files
del /q main\generated\generated_ui_components.h 2>nul
del /q main\generated\generated_component_factories.cpp 2>nul

REM Regenerate
python tools\process_manifests.py --project-root . --output-dir main\generated

REM Clean build
call idf.py fullclean
call idf.py build

pause
```

## Still having issues?

1. Enable verbose output:
```cmd
idf.py -v build
```

2. Check component dependencies:
```cmd
idf.py show-components
```

3. Validate all JSON files:
```cmd
python tools\validate_manifests.py
```

---

**Remember: Phase 5 is cutting-edge - some assembly required!** 🚀
