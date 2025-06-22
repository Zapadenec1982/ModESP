# Getting Started - Developer Guide

## 🚀 Quick Setup (15 minutes)

### Prerequisites
- Ubuntu 20.04+ / Windows 10+ / macOS 10.15+
- Git installed
- 8GB+ RAM, 10GB+ free disk space
- ESP32-S3 development board (recommended)
- VSCode (recommended IDE)

### 1. Environment Setup

#### Install ESP-IDF
```bash
# Linux/macOS
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout v5.0.4  # Use stable version
./install.sh esp32s3
source export.sh

# Add to shell profile
echo 'alias get_idf=". ~/esp/esp-idf/export.sh"' >> ~/.bashrc
```

#### Windows (Using ESP-IDF Installer)
1. Download ESP-IDF installer from espressif.com
2. Install with ESP32-S3 support
3. Launch ESP-IDF PowerShell

#### Verify Installation
```bash
idf.py --version
# Should show: ESP-IDF v5.0.4
```

### 2. Clone Project
```bash
cd ~/projects  # or your preferred directory
git clone <repository-url> ModESP_dev
cd ModESP_dev

# Initialize submodules
git submodule update --init --recursive
```

### 3. Configure Project
```bash
# Set target
idf.py set-target esp32s3

# Configure options
idf.py menuconfig
```

#### Key Configuration Options
Navigate to:
- **ModuChill Configuration** → Enable desired modules
- **Component config** → ESP32S3-specific settings
- **Serial flasher config** → Set correct COM port

### 4. First Build
```bash
# Clean build
idf.py fullclean

# Build project
idf.py build
```

Expected output:
```
Project build complete. To flash, run this command:
idf.py -p (PORT) flash
```

### 5. Flash and Test
```bash
# Find your ESP32 port
# Linux: usually /dev/ttyUSB0 or /dev/ttyACM0
# Windows: usually COM3, COM4, etc.
# macOS: usually /dev/cu.usbserial-*

# Flash firmware
idf.py -p /dev/ttyUSB0 flash monitor
```

You should see output like:
```
I (123) Main: ModuChill starting...
I (456) Core: System initialized successfully
I (789) HAL: Board configuration loaded
```

## 🛠️ Development Workflow

### Daily Development Cycle

#### 1. Start Development Session
```bash
# Get latest changes
git checkout main
git pull origin main

# Check current TODO items
cat Docs/TODO.md | grep "Не розпочато" | head -5

# Create feature branch
git checkout -b feature/TODO-XXX-description
```

#### 2. AI-Assisted Development
```bash
# Open project in VSCode
code .

# Use Desktop Commander for file operations
# Use AI assistant for code generation
# Regular builds and tests
idf.py build && idf.py flash monitor
```

#### 3. Testing and Validation
```bash
# Run unit tests (when available)
idf.py pytest

# Check memory usage
idf.py size-components

# Monitor performance
idf.py monitor --print_filter "*:V"
```

#### 4. Commit and Push
```bash
# Stage changes
git add .

# Commit with proper format
git commit -m "feat(core): TODO-XXX implement feature

- Add implementation details
- Include test results
- Note AI collaboration"

# Push regularly
git push origin feature/TODO-XXX-description
```
### Project Structure Understanding

```
ModESP_dev/
├── main/                    # ESP-IDF entry point
│   ├── main.c              # C entry point
│   └── CMakeLists.txt      # Build config
├── components/             # Modular components
│   ├── core/              # Core system (EventBus, SharedState)
│   ├── ESPhal/            # Hardware abstraction layer
│   ├── sensor_drivers/    # Sensor driver modules
│   ├── actuator_drivers/  # Actuator driver modules
│   ├── ui/                # Web interface
│   └── wifi_manager/      # Network connectivity
├── Docs/                  # Project documentation
│   ├── README.md          # Documentation index
│   ├── TODO.md            # Development tasks
│   └── *.md               # Various documentation
├── CMakeLists.txt         # Main build configuration
├── sdkconfig              # ESP-IDF configuration
└── README.md              # Project overview
```

## 🎯 First Contribution Guide

### Choose Your First Task

#### Beginner Tasks (1-2 hours)
```bash
# Look for these in TODO.md:
grep -A 5 "✅ TODO-004" Docs/TODO.md  # Configuration Validator
grep -A 5 "✅ TODO-005" Docs/TODO.md  # Performance Profiling
```

#### Intermediate Tasks (3-4 hours)
```bash
grep -A 5 "✅ TODO-001" Docs/TODO.md  # ModuleHeartbeat
grep -A 5 "✅ TODO-008" Docs/TODO.md  # WiFi Manager
```

#### Advanced Tasks (5+ hours)
```bash
grep -A 5 "✅ TODO-012" Docs/TODO.md  # PID Controller
grep -A 5 "✅ TODO-010" Docs/TODO.md  # Safety Systems
```

### Development Process with AI

#### Step 1: Understand Requirements
```bash
# Read related documentation
cat Docs/Core.txt | grep -A 10 "ModuleHeartbeat"

# Check existing code
find components/ -name "*.h" -o -name "*.cpp" | xargs grep -l "heartbeat"

# Review architecture
cat Docs/SYSTEM_ARCHITECTURE.md
```

#### Step 2: AI Collaboration Session
1. **Describe the task** to AI assistant
2. **Ask for architecture** proposal
3. **Generate initial code** structure
4. **Iterate on implementation** with AI
5. **Test and validate** results

#### Step 3: Implementation
```cpp
// Example: Create new component
mkdir -p components/my_component/{include,src,test}
touch components/my_component/CMakeLists.txt
touch components/my_component/include/my_component.h
touch components/my_component/src/my_component.cpp
```

#### Step 4: Testing
```bash
# Build and test frequently
idf.py build
idf.py flash monitor

# Check memory impact
idf.py size-components | grep my_component
```

#### Step 5: Documentation and Commit
```bash
# Update documentation
echo "## MyComponent" >> Docs/README.md
echo "Brief description" >> Docs/README.md

# Commit with proper message
git commit -m "feat(my_component): TODO-XXX complete implementation

Co-authored-by: AI Assistant <ai@assistant.com>

- Implemented core functionality
- Added comprehensive tests
- Validated on hardware

Closes: TODO-XXX"
```

## 🔧 Development Tools

### VSCode Setup

#### Required Extensions
```json
{
    "recommendations": [
        "ms-vscode.cpptools",
        "ms-vscode.cmake-tools",
        "espressif.esp-idf-extension",
        "ms-python.python",
        "redhat.vscode-yaml"
    ]
}
```

#### Useful Settings
```json
{
    "C_Cpp.default.compilerPath": "${env:IDF_PATH}/tools/xtensa-esp32s3-elf/esp-2021r2-patch3-8.4.0/xtensa-esp32s3-elf/bin/xtensa-esp32s3-elf-gcc",
    "files.associations": {
        "*.h": "c",
        "*.cpp": "cpp"
    },
    "editor.formatOnSave": true,
    "editor.rulers": [80, 120],
    "files.trimTrailingWhitespace": true
}
```

### Hardware Setup

#### Recommended Development Board
- **ESP32-S3-DevKitC-1** (official Espressif board)
- **USB-C cable** for programming and power
- **Breadboard** for sensor connections
- **Jumper wires** for prototyping

#### Basic Sensor Setup
```
ESP32-S3 Pin Connections:
├── GPIO4  → OneWire Data (DS18B20 sensors)
├── GPIO5  → Relay Control Output
├── GPIO6  → PWM Output (fans, valves)
├── GPIO7  → Digital Input (door switch)
├── ADC1_0 → Analog Input (NTC thermistor)
└── I2C    → GPIO8 (SDA), GPIO9 (SCL)
```

### Debugging Tools

#### Serial Monitor
```bash
# Monitor with filters
idf.py monitor --print_filter "*:I"     # Info and above
idf.py monitor --print_filter "Core:V"  # Core module verbose
```

#### Memory Analysis
```bash
# Check component sizes
idf.py size-components

# Check memory map
idf.py size

# Runtime memory monitoring
# Add to code: ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());
```

#### Performance Profiling
```bash
# Enable profiling in menuconfig
# Component config → ESP32S3-specific → CPU frequency → 240MHz

# Add timing measurements in code
uint64_t start = esp_timer_get_time();
// ... operation ...
uint64_t duration = esp_timer_get_time() - start;
ESP_LOGI("PERF", "Operation took %llu μs", duration);
```

## 📚 Learning Resources

### Essential Reading
1. **Project Documentation**
   - [SYSTEM_ARCHITECTURE.md](Docs/SYSTEM_ARCHITECTURE.md)
   - [Core.txt](Docs/Core.txt)
   - [TODO.md](Docs/TODO.md)

2. **ESP-IDF Documentation**
   - [ESP-IDF Programming Guide](https://docs.espressif.com/projects/esp-idf/en/latest/)
   - [FreeRTOS Guide](https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/system/freertos.html)

3. **Component Examples**
   - Look at existing `sensor_drivers/` for patterns
   - Study `core/` for architectural patterns

### Common Patterns

#### Component Creation Pattern
```cpp
// 1. Define interface
class IMyInterface {
public:
    virtual bool initialize() = 0;
    virtual void update() = 0;
    virtual ~IMyInterface() = default;
};

// 2. Implement component
class MyComponent : public IMyInterface {
public:
    bool initialize() override;
    void update() override;
private:
    static constexpr const char* TAG = "MyComponent";
};

// 3. Register with system
// In module_manager.cpp:
// register_module(std::make_unique<MyComponent>(), ModulePriority::NORMAL);
```

#### Error Handling Pattern
```cpp
std::optional<float> read_sensor() {
    float value;
    esp_err_t err = hardware_read(&value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Sensor read failed: %s", esp_err_to_name(err));
        return std::nullopt;
    }
    return value;
}
```

## 🚨 Common Issues and Solutions

### Build Issues
```bash
# Permission denied
sudo chmod +x ~/esp/esp-idf/install.sh

# Python environment
pip install --user -r ~/esp/esp-idf/requirements.txt

# Clean build
idf.py fullclean && idf.py build
```

### Flash Issues
```bash
# Wrong port
ls /dev/tty*  # Linux
# or check Device Manager (Windows)

# Permission issues (Linux)
sudo usermod -a -G dialout $USER
# Then logout/login

# Erase flash if corrupted
idf.py erase_flash
```

### Runtime Issues
```bash
# Check logs
idf.py monitor

# Enable verbose logging
idf.py menuconfig
# Component config → Log output → Default log verbosity → Verbose
```

## 🎉 Success Checklist

After completing your first task:
- [ ] Code compiles without warnings
- [ ] Flashes and runs on ESP32-S3
- [ ] Meets performance requirements
- [ ] Has proper documentation
- [ ] Follows coding standards
- [ ] Git commit follows format
- [ ] Ready for code review

Welcome to the ModuChill development team! 🎯