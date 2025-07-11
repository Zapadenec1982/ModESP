# 📚 Build Instructions for ModESP

This directory contains comprehensive build instructions for the ModESP project.

## 🚀 Quick Start

**New to ModESP?** Start here:
1. Read [`QUICK_BUILD_GUIDE.md`](QUICK_BUILD_GUIDE.md) - 5-minute build guide
2. Use the build scripts in project root:
   - `build.bat` - Automated build script
   - `flash.bat` - Flash to ESP32

## 📋 Available Documentation

### [`BUILD_CHEATSHEET.md`](BUILD_CHEATSHEET.md) ⭐
**Start here!** Ultra-quick reference:
- 3-step build process
- One-liner commands
- Common fixes

### [`BUILD_INSTRUCTIONS.md`](BUILD_INSTRUCTIONS.md)
Complete step-by-step build instructions including:
- ESP-IDF setup
- Configuration options
- Build commands
- Troubleshooting

### [`QUICK_BUILD_GUIDE.md`](QUICK_BUILD_GUIDE.md)
Fast-track guide for experienced developers:
- Pre-made scripts usage
- One-command build & flash
- Quick troubleshooting

### [`PHASE5_BUILD_TROUBLESHOOTING.md`](PHASE5_BUILD_TROUBLESHOOTING.md)
Specific issues related to Phase 5 Adaptive UI:
- Component not found errors
- Include path problems
- Stub implementations
- Quick fixes

## 🔧 Build Scripts (in project root)

### Windows Batch Files
- **`build.bat`** - Full build with ESP-IDF check
- **`flash.bat`** - Interactive flash and monitor
- **`build.ps1`** - PowerShell alternative

## 💡 Common Commands

### Basic Build
```bash
cd C:\ModESP_dev
idf.py build
```

### Flash & Monitor
```bash
idf.py -p COM3 flash monitor
```

### Clean Build
```bash
idf.py fullclean
idf.py build
```

## ⚡ Fastest Build Path

1. Open **ESP-IDF 5.x CMD** from Start Menu
2. Navigate to project:
   ```cmd
   cd C:\ModESP_dev
   ```
3. Run build script:
   ```cmd
   build.bat
   ```
4. Flash to ESP32:
   ```cmd
   flash.bat
   ```

## 🛠️ Prerequisites

Before building, ensure you have:
- ✅ ESP-IDF 4.4+ installed
- ✅ Python 3.8+ 
- ✅ Git
- ✅ USB drivers for your ESP32 board

## 📊 Expected Build Time

- First build: 5-10 minutes
- Subsequent builds: 1-3 minutes
- Clean build: 5-10 minutes

## ❓ Need Help?

1. Check [`PHASE5_BUILD_TROUBLESHOOTING.md`](PHASE5_BUILD_TROUBLESHOOTING.md)
2. Enable verbose output: `idf.py -v build`
3. Check component list: `idf.py show-components`

---

**Happy Building!** 🚀
