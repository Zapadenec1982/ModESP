# 🔧 ModESP Build Instructions - Windows

## Step 1: Відкрийте ESP-IDF Command Prompt

### Варіант A: З меню Пуск
1. Натисніть Win клавішу
2. Введіть "ESP-IDF" 
3. Виберіть "ESP-IDF 5.x CMD" або "ESP-IDF 4.x PowerShell"

### Варіант B: З папки ESP-IDF
1. Перейдіть до папки встановлення ESP-IDF (наприклад C:\Espressif\)
2. Запустіть `idf_cmd_init.bat`

### Варіант C: VS Code з ESP-IDF extension
1. Відкрийте VS Code
2. Ctrl+Shift+P → "ESP-IDF: Open ESP-IDF Terminal"

## Step 2: Перейдіть до проекту
```cmd
cd C:\ModESP_dev
```

## Step 3: Налаштуйте target (ESP32)
```cmd
idf.py set-target esp32
```

## Step 4: Налаштуйте проект (опціонально)
```cmd
idf.py menuconfig
```
Тут можна налаштувати:
- WiFi credentials
- Розмір flash
- Logging level
- Component settings

Для виходу: Q → Y

## Step 5: Очистіть попередні build файли
```cmd
idf.py fullclean
```

## Step 6: Компіляція проекту
```cmd
idf.py build
```

Це займе 3-10 хвилин при першій компіляції.

## Step 7: Прошивка на ESP32
Підключіть ESP32 до комп'ютера через USB.

```cmd
idf.py -p COM3 flash
```
Замініть COM3 на ваш порт (можна побачити в Device Manager)

## Step 8: Моніторинг serial output
```cmd
idf.py -p COM3 monitor
```
Для виходу: Ctrl+]

## Або все разом:
```cmd
idf.py -p COM3 flash monitor
```

## 🛠️ Troubleshooting

### Помилка: "idf.py not found"
- Переконайтеся що використовуєте ESP-IDF Command Prompt
- Або додайте ESP-IDF до PATH

### Помилка: "CMake Error"
```cmd
idf.py reconfigure
```

### Помилка: "Component not found"
```cmd
idf.py update-dependencies
```

### Помилка компіляції adaptive_ui
1. Перевірте що всі файли на місці:
   - components/adaptive_ui/CMakeLists.txt
   - components/adaptive_ui/include/*.h
   - components/adaptive_ui/*.cpp

2. Перевірте includes в CMakeLists.txt

### Порт не знайдено
1. Перевірте в Device Manager який COM порт
2. Встановіть драйвери CH340/CP2102 якщо потрібно

## 📊 Очікуваний результат

Успішна компіляція виглядає так:
```
Project build complete. To flash, run:
 idf.py flash
or
 idf.py -p PORT flash
```

Після прошивки в моніторі побачите:
```
I (1234) ModESP: System starting...
I (1235) AdaptiveUI: Initializing Phase 5 architecture
I (1236) SensorManager: Starting with 0 drivers
...
```

## 🎯 Quick Build Script

Створіть `build.bat`:
```batch
@echo off
echo Building ModESP...
cd C:\ModESP_dev
call idf.py build
if %ERRORLEVEL% == 0 (
    echo Build successful!
    echo Run 'idf.py -p COM3 flash monitor' to upload
) else (
    echo Build failed!
)
pause
```
