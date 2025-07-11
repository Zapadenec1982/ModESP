# 🎯 ModESP Build Cheat Sheet

## 🚀 Super Quick (3 steps)

1. **Open:** ESP-IDF 5.x CMD (from Start Menu)
2. **Navigate:** `cd C:\ModESP_dev`
3. **Build:** `build.bat`

Done! 🎉

## ⚡ One-Liner (if configured)

```cmd
cd C:\ModESP_dev && idf.py -p COM3 flash monitor
```

## 📋 Step by Step

### 1. Open Terminal
```
Start Menu → "ESP-IDF" → "ESP-IDF 5.x CMD"
```

### 2. Go to Project
```cmd
cd C:\ModESP_dev
```

### 3. Build
```cmd
idf.py build
```

### 4. Flash
```cmd
idf.py -p COM3 flash
```

### 5. Monitor
```cmd
idf.py -p COM3 monitor
```

Exit monitor: `Ctrl+]`

## 🛠️ Common Fixes

### Build Error?
```cmd
idf.py fullclean
idf.py build
```

### Component Error?
```cmd
python tools\process_manifests.py --project-root . --output-dir main\generated
idf.py build
```

### Wrong Port?
Check Device Manager → Ports (COM & LPT)

## 🔧 Useful Commands

| What | Command |
|------|---------|
| Clean | `idf.py clean` |
| Full Clean | `idf.py fullclean` |
| Config Menu | `idf.py menuconfig` |
| Size Info | `idf.py size` |
| Just Flash | `idf.py flash` |
| Just Monitor | `idf.py monitor` |

---

**That's it! Your ModESP is building!** 🚀
