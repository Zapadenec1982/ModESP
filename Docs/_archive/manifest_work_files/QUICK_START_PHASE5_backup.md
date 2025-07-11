# Quick Start: Phase 5 Adaptive UI Implementation

## 🚀 Швидкий старт для розробників

### Крок 1: Налаштування середовища

```bash
# Перейти в директорію проекту
cd C:\ModESP_dev

# Створити гілку для Phase 5
git checkout -b phase5-adaptive-ui

# Перевірити що всі залежності встановлені
idf.py fullclean
idf.py build
```

### Крок 2: Додати нові файли Phase 5

Створіть наступні файли в `components/core/`:
- `base_driver.h` - базовий клас для драйверів ✅
- `ui_component_base.h` - базові UI компоненти ✅
- `ui_filter.h` - фільтрація компонентів ✅
- `lazy_component_loader.h` - lazy loading ✅
- `module_manager_adaptive.h` - розширення ModuleManager ✅

### Крок 3: Оновити маніфести

#### 3.1 Оновити SensorModule маніфест
```json
// components/sensor_drivers/module_manifest.json
{
  "module": {
    "name": "SensorModule",
    "type": "MANAGER",  // Змінити на MANAGER
    "driver_interface": "ISensorDriver"  // Додати
  },
  
  "ui": {
    "adaptive": {  // Нова секція
      "components": [
        {
          "id": "sensor_overview",
          "type": "composite",
          "conditions": ["always"],
          "access_level": "user",
          "priority": "high",
          "lazy_load": false
        }
      ]
    }
  }
}
```

#### 3.2 Оновити DS18B20 драйвер маніфест
```json
// components/sensor_drivers/ds18b20_async/ds18b20_driver_manifest.json
{
  "ui_extensions": {
    "inject_into": "sensor_config_panel",
    "components": [
      {
        "id": "ds18b20_resolution_slider",
        "type": "slider",
        "condition": "config.sensor.type == 'DS18B20'",
        "access_level": "technician",
        "config": {
          "min": 9,
          "max": 12,
          "label": "Resolution (bits)"
        }
      }
    ]
  }
}
```

### Крок 4: Розширити process_manifests.py

```python
# tools/process_manifests.py - додати в клас ManifestProcessor

def generate_adaptive_ui_components(self):
    """Generate all possible UI components for Phase 5"""
    
    components = []
    
    # Process manager UI components
    for module in self.modules:
        if module.get('type') == 'MANAGER':
            ui_adaptive = module.get('ui', {}).get('adaptive', {})
            components.extend(ui_adaptive.get('components', []))
    
    # Process driver UI extensions
    for driver in self.drivers:
        ui_ext = driver.get('ui_extensions', {})
        components.extend(ui_ext.get('components', []))
    
    # Generate C++ code
    self._generate_component_registry(components)
    self._generate_component_factories(components)
```

### Крок 5: Створити тестовий приклад

```cpp
// main/test_adaptive_ui.cpp
#include "ui_filter.h"
#include "lazy_component_loader.h"
#include "esp_log.h"

static const char* TAG = "AdaptiveUITest";

void test_adaptive_ui() {
    // 1. Ініціалізація
    ModESP::UI::UIFilter filter;
    ModESP::UI::LazyComponentLoader loader;
    
    // 2. Конфігурація
    nlohmann::json config = {
        {"sensor", {
            {"type", "DS18B20"},
            {"count", 2}
        }}
    };
    
    // 3. Фільтрація
    filter.init(config, ModESP::UI::UserRole::TECHNICIAN);
    
    // 4. Завантаження компонентів
    auto* component = loader.getComponent("ds18b20_resolution_slider");
    if (component) {
        ESP_LOGI(TAG, "Component loaded: %s", component->getId().c_str());
    }
    
    // 5. Статистика
    auto stats = loader.getStats();
    ESP_LOGI(TAG, "Loaded: %zu, Cache: %zu bytes, Hit rate: %.2f%%",
             stats.components_loaded, 
             stats.cache_size_bytes,
             stats.hit_rate * 100);
}
```

### Крок 6: Компіляція та тестування

```bash
# Регенерувати код з маніфестів
python tools/process_manifests.py \
    --project-root . \
    --output-dir main/generated

# Компіляція
idf.py build

# Прошивка та моніторинг
idf.py flash monitor
```

## 📊 Очікувані результати

### Початковий стан
```
I (1234) ModuleManager: Loading UI components...
I (1235) UIFilter: Total components: 25
I (1236) UIFilter: Visible components: 8
I (1237) LazyLoader: Priority components preloaded: 3
```

### Після зміни конфігурації
```
I (5678) Config: sensor.type changed to 'NTC'
I (5679) UIFilter: Re-filtering components...
I (5680) UIFilter: Visible components: 6 (was 8)
I (5681) LazyLoader: Loading component: ntc_resistance_input
I (5682) UI: Menu updated in 5ms
```

## 🔧 Debugging

### Включити детальні логи
```cpp
// В menuconfig або через код
esp_log_level_set("UIFilter", ESP_LOG_DEBUG);
esp_log_level_set("LazyLoader", ESP_LOG_DEBUG);
```

### Перевірити згенеровані файли
```bash
# Переглянути всі згенеровані компоненти
cat main/generated/generated_ui_components.h

# Перевірити реєстрацію факторій
cat main/generated/generated_component_factories.cpp
```

## 🎯 Checklist для початку

- [ ] Створити гілку phase5-adaptive-ui
- [ ] Додати нові header файли (base_driver.h, etc.)
- [ ] Оновити manifest для SensorModule
- [ ] Розширити process_manifests.py
- [ ] Створити тестовий приклад
- [ ] Запустити та перевірити результати

## 📚 Додаткові ресурси

- [ADAPTIVE_UI_ARCHITECTURE.md](ADAPTIVE_UI_ARCHITECTURE.md) - детальна архітектура
- [PHASE5_IMPLEMENTATION_GUIDE.md](PHASE5_IMPLEMENTATION_GUIDE.md) - повний план
- [Приклади компонентів](../examples/) - референсні реалізації

---

**Ready to revolutionize UI generation? Let's go!** 🚀
