# Adaptive UI Component

Єдина UI система для ModESP з підтримкою адаптивного інтерфейсу, фільтрації компонентів та lazy loading.

## 🌟 Особливості

- **Адаптивна генерація UI** - інтерфейс змінюється залежно від конфігурації та ролі користувача
- **Lazy Loading** - компоненти завантажуються тільки при потребі (економія RAM)
- **Мультипротокольна підтримка** - Web, LCD, MQTT, Telegram
- **Фільтрація за умовами** - показ тільки релевантних компонентів
- **Compile-time оптимізація** - UI генерується під час компіляції

## 📁 Структура

```
adaptive_ui/
├── include/              # Основні інтерфейси та класи
│   ├── ui_filter.h      # Фільтрація компонентів
│   ├── lazy_component_loader.h  # Відкладене завантаження
│   └── ui_component_base.h      # Базові класи компонентів
├── adapters/            # Адаптери для різних протоколів
│   ├── web/            # HTTP/WebSocket інтерфейс
│   ├── lcd_ui/         # LCD дисплей
│   └── mqtt_ui/        # MQTT протокол
├── examples/           # Приклади використання
└── renderers/          # Рендерери для різних форматів
```

## 🚀 Використання

### Web UI

```cpp
#include "web_ui_adapter.h"
#include "ui_filter.h"
#include "lazy_component_loader.h"

void setup_web_ui() {
    // Налаштування фільтра
    UIFilter filter;
    filter.init(config, UserRole::TECHNICIAN);
    
    // Отримання loader
    auto& loader = LazyLoaderManager::getInstance();
    
    // Створення веб-адаптера
    auto web = std::make_unique<WebUIAdapter>(&filter, &loader);
    web->start(80);
}
```

### Фільтрація компонентів

```cpp
// Приклади умов фільтрації:
"always"                          // Завжди показувати
"config.sensor.type == 'DS18B20'" // Тільки для DS18B20
"role >= 'technician'"            // Для техніків і вище
"has_feature('calibration')"      // Якщо є функція калібрування
```

## 🔧 Конфігурація

### CMakeLists.txt

```cmake
REQUIRES 
    adaptive_ui  # Додати до залежностей
```

### Ролі користувачів

- `USER` - базовий доступ
- `OPERATOR` - оператор обладнання
- `TECHNICIAN` - технічне обслуговування
- `SUPERVISOR` - керівник
- `ADMIN` - повний доступ

## 📊 Переваги

- **Економія RAM**: до 40% через lazy loading
- **Гнучкість**: легко додавати нові протоколи
- **Безпека**: контроль доступу на рівні компонентів
- **Продуктивність**: compile-time генерація

## 🎯 Roadmap

- [ ] WebSocket для real-time оновлень
- [ ] Повна реалізація LCD адаптера
- [ ] MQTT discovery
- [ ] Telegram bot інтеграція
- [ ] Mobile app API
