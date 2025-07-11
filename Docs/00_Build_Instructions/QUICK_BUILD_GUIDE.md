# 🚀 Покрокова компіляція ModESP - Швидкий старт

## Варіант 1: Використання готових скриптів (Найпростіший)

### 1️⃣ Відкрийте ESP-IDF Command Prompt
- Натисніть клавішу **Windows**
- Введіть: **ESP-IDF**
- Виберіть: **ESP-IDF 5.x CMD** або **ESP-IDF 4.x CMD**

### 2️⃣ Перейдіть до проекту
```cmd
cd C:\ModESP_dev
```

### 3️⃣ Запустіть build скрипт
```cmd
build.bat
```

### 4️⃣ Прошийте ESP32
```cmd
flash.bat
```
Введіть номер COM порту (наприклад COM3)

---

## Варіант 2: Ручна компіляція (Детальний контроль)

### 1️⃣ Відкрийте ESP-IDF Terminal
Один з варіантів:
- **Start Menu** → ESP-IDF 5.x CMD
- **VS Code** → Ctrl+Shift+P → "ESP-IDF: Open ESP-IDF Terminal"
- **Папка Espressif** → запустіть idf_cmd_init.bat

### 2️⃣ Перевірте що ESP-IDF працює
```cmd
idf.py --version
```
Має показати версію (наприклад ESP-IDF v5.1.2)

### 3️⃣ Перейдіть до проекту
```cmd
cd C:\ModESP_dev
```

### 4️⃣ Налаштуйте target
```cmd
idf.py set-target esp32
```

### 5️⃣ (Опціонально) Налаштуйте параметри
```cmd
idf.py menuconfig
```
- Використовуйте стрілки для навігації
- Enter для входу в меню
- Space для вибору опцій
- Q для виходу, Y для збереження

### 6️⃣ Компіляція
```cmd
idf.py build
```

**Очікуваний результат:**
```
[100%] Built target __ldgen_output_esp32.project.ld
[100%] Linking CXX executable ModESP.elf
[100%] Built target ModESP.elf
[100%] Generating binary image from built executable

Project build complete. To flash, run:
 idf.py flash
```

### 7️⃣ Підключіть ESP32
1. Підключіть ESP32 до USB
2. Перевірте COM порт:
   - Device Manager → Ports (COM & LPT)
   - Шукайте "Silicon Labs CP210x" або "CH340"

### 8️⃣ Прошивка
```cmd
idf.py -p COM3 flash
```
Замініть COM3 на ваш порт

### 9️⃣ Моніторинг
```cmd
idf.py -p COM3 monitor
```

**Очікуваний вивід:**
```
I (325) cpu_start: Starting app cpu, entry point is 0x40081234
I (0) cpu_start: App cpu up.
I (342) heap_init: Initializing. RAM available for heap:
I (1234) ModESP: System starting...
I (1235) AdaptiveUI: Phase 5 Architecture Initialized
I (1236) SensorManager: Ready with 0 drivers
I (1237) ModESP: System ready!
```

Для виходу з монітора: **Ctrl+]**

---

## 🛠️ Швидке вирішення проблем

### ❌ "idf.py not found"
→ Використовуйте ESP-IDF Command Prompt, не звичайний cmd

### ❌ "Project not found"  
→ Перевірте що ви в папці C:\ModESP_dev

### ❌ "Component adaptive_ui not found"
→ Перевірте що існує папка components/adaptive_ui/

### ❌ "Serial port COM3 not found"
→ Перевірте правильний COM порт в Device Manager

### ❌ Build errors
1. Очистіть попередній build:
   ```cmd
   idf.py fullclean
   ```
2. Перекомпілюйте:
   ```cmd
   idf.py build
   ```

---

## ⚡ Супер-швидкий старт (1 команда)

Якщо все налаштовано, можна зробити все однією командою:
```cmd
idf.py -p COM3 flash monitor
```

Це:
1. Компілює проект (якщо потрібно)
2. Прошиває ESP32
3. Відкриває монітор

---

## 📝 Корисні команди

| Команда | Опис |
|---------|------|
| `idf.py build` | Компіляція |
| `idf.py clean` | Часткове очищення |
| `idf.py fullclean` | Повне очищення |
| `idf.py flash` | Прошивка |
| `idf.py monitor` | Серійний монітор |
| `idf.py menuconfig` | Налаштування |
| `idf.py size` | Розмір прошивки |
| `idf.py docs` | Відкрити документацію |

---

**Готово! Ваш ModESP з Phase 5 Adaptive UI готовий до роботи!** 🚀
