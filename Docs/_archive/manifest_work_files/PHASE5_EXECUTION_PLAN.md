# 🚀 Phase 5 Step-by-Step Execution Plan

## Оптимальний порядок виконання (Day 1)

### ✅ Step 1: Update Manifests (15 хв)
**Чому перше**: Без маніфестів нічого не працюватиме

```bash
# 1.1 Відкрити sensor module manifest
code C:\ModESP_dev\components\sensor_drivers\module_manifest.json

# Додати після "name": "SensorModule":
"type": "MANAGER",
"driver_interface": "ISensorDriver"

# Додати нову секцію ui.adaptive

# 1.2 Відкрити DS18B20 driver manifest  
code C:\ModESP_dev\components\sensor_drivers\ds18b20_async\ds18b20_driver_manifest.json

# Додати ui_extensions секцію з прикладу
```

### ✅ Step 2: Integrate Adaptive UI Generator (30 хв)
**Чому друге**: Потрібно для генерації компонентів

```bash
# 2.1 Відкрити process_manifests.py
code C:\ModESP_dev\tools\process_manifests.py

# 2.2 Додати на початок файлу:
from adaptive_ui_generator import generate_adaptive_ui

# 2.3 В методі generate_code() додати після існуючої генерації:
# Generate adaptive UI components
print("Generating adaptive UI components...")
generate_adaptive_ui(
    self.modules,
    self.drivers, 
    self.output_dir
)
```

### ✅ Step 3: Test Generation (10 хв)
**Чому третє**: Швидка перевірка що все працює

```bash
cd C:\ModESP_dev

# Запустити генерацію
python tools\process_manifests.py --project-root . --output-dir main\generated

# Перевірити результат
dir main\generated\generated_ui_*
```

### ✅ Step 4: Fix Compilation Errors (30 хв)
**Чому четверте**: Потрібно щоб проект компілювався

```bash
# 4.1 Спробувати компіляцію
idf.py build

# 4.2 Якщо помилки - виправити:
# - Додати відсутні #include
# - Виправити namespace
# - Додати forward declarations
```

### ✅ Step 5: Run Basic Test (20 хв)
**Чому п'яте**: Валідація що система працює

```bash
# 5.1 Додати виклик тесту в app_main
code C:\ModESP_dev\main\app_main.cpp

# Додати:
extern "C" void test_adaptive_ui();
// В app_main():
test_adaptive_ui();

# 5.2 Flash та monitor
idf.py flash monitor
```

## 📊 Очікувані результати кожного кроку

### After Step 1:
- ✓ Маніфести оновлені з adaptive UI

### After Step 2:
- ✓ process_manifests.py розширений

### After Step 3:
- ✓ Файли згенеровані:
  - generated_ui_components.h
  - generated_component_factories.cpp

### After Step 4:
- ✓ Проект компілюється без помилок

### After Step 5:
- ✓ В логах видно:
```
I (1234) AdaptiveUITest: === Phase 5 Adaptive UI Test ===
I (1235) AdaptiveUITest: Filtering 5 total components...
I (1236) AdaptiveUITest: Visible components: 3
```

## 🔧 Troubleshooting

### Якщо генерація не працює:
1. Перевірте чи імпортований adaptive_ui_generator
2. Додайте print() для debug
3. Запустіть з --verbose

### Якщо компіляція failed:
1. Перевірте всі #include
2. Додайте stub implementations
3. Закоментуйте проблемні частини

### Якщо тест не запускається:
1. Перевірте чи викликається test_adaptive_ui()
2. Додайте ESP_LOGI на початку
3. Спростіть тест до мінімуму

## ⏱️ Time estimate: 2-3 години

**Головне - робити покроково та перевіряти результат після кожного кроку!**

---

Ready? Let's start with Step 1! 🚀
