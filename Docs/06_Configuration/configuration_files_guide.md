# ModESP Configuration Files Guide

## 📋 Огляд

ModESP використовує JSON-файли конфігурації для різних типів холодильного обладнання. Всі конфігурації вбудовуються в прошивку під час компіляції.

## 🗂️ Доступні конфігурації

### default_config.json - Стандартний холодильник
- **Діапазон температур**: -10°C до +10°C  
- **Тип керування**: Одноступенчатий
- **Цикл розморожування**: Базовий

### freezer_config.json - Комерційна морозильна камера
- **Діапазон температур**: -30°C до -10°C
- **Цикли розморожування**: Продовжені
- **Компресор**: Знижена частота включень

### ripening_config.json - Камера дозрівання
- **Діапазон температур**: +10°C до +20°C
- **Контроль вологості**: 85% ± 3%
- **Моніторинг CO2**: Включений
- **Криві дозрівання**: Попередньо запрограмовані

## 🏗️ Структура конфігурації

```json
{
    "version": 1,                    // Версія конфігурації для міграції
    "system": {                      // Системні налаштування
        "name": "Назва обладнання",
        "type": "тип_обладнання"
    },
    "climate": {                     // Контроль температури
        "setpoint": 4.0,             // Уставка температури
        "hysteresis": 0.5            // Гістерезис
    },
    "compressor": {                  // Захист компресора
        "min_off_time": 180,         // Мін. час вимкнення (сек)
        "min_on_time": 120           // Мін. час роботи (сек)
    },
    "sensors": {                     // Конфігурація датчиків
        "temperature": {
            "type": "DS18B20",
            "pin": 4,
            "resolution": 12
        }
    },
    "actuators": {                   // Конфігурація актуаторів
        "compressor": {
            "pin": 2,
            "active_level": "HIGH"
        }
    }
}
```

## 💻 Використання в коді

```cpp
// Завантажити конфігурацію морозильника
ConfigManager::load_type("freezer");

// Отримати поточний тип
std::string type = ConfigManager::get_type();

// Скинути до заводських налаштувань
ConfigManager::reset_to_defaults();

// Отримати значення з конфігурації
float setpoint = ConfigManager::get<float>("climate.setpoint");
```

## ✅ Додавання власних конфігурацій

### 1. Створіть JSON файл
```bash
# У папці components/core/configs/
touch my_equipment_config.json
```

### 2. Слідуйте структурі існуючих конфігурацій
```json
{
    "version": 1,
    "system": {
        "name": "My Equipment",
        "type": "my_equipment"
    },
    // ... інші налаштування
}
```

### 3. Додайте файл до CMakeLists.txt
```cmake
# У components/core/CMakeLists.txt
set(CONFIG_FILES
    "configs/default_config.json"
    "configs/freezer_config.json"
    "configs/ripening_config.json"
    "configs/my_equipment_config.json"  # Додайте свій файл
)
```

### 4. Перезберіть проект
```bash
idf.py build
```

## 🔧 Best Practices

### 1. **Версіонування конфігурацій**
```json
{
    "version": 2,  // Збільшуйте при кардинальних змінах
    // ...
}
```

### 2. **Документування змін**
```json
{
    // Компресор: збільшено мін. час для зменшення зносу
    "min_off_time": 240  // було 180
}
```

### 3. **Безпечні значення за замовчуванням**
```json
{
    "climate": {
        "setpoint": 4.0,      // Безпечна температура
        "max_setpoint": 15.0, // Захист від перегрівання
        "min_setpoint": -25.0 // Захист від переохолодження
    }
}
```

### 4. **Валідація діапазонів**
```json
{
    "sensors": {
        "temperature": {
            "min_value": -50.0,  // Технічні межі датчика
            "max_value": 85.0,
            "error_threshold": 3.0  // Поріг помилки
        }
    }
}
```

## 🛡️ Валідація конфігурацій

### Рівні валідації:
1. **Час компіляції**: Синтаксис JSON
2. **Час завантаження**: Обов'язкові поля
3. **Час виконання**: Діапазони значень

### Приклад валідації:
```cpp
// У config_manager.cpp
bool ConfigManager::validate_climate_config(const json& config) {
    // Перевірка обов'язкових полів
    if (!config.contains("setpoint")) {
        ESP_LOGE(TAG, "Missing 'setpoint' in climate config");
        return false;
    }
    
    // Перевірка діапазонів
    float setpoint = config["setpoint"];
    if (setpoint < -50.0 || setpoint > 50.0) {
        ESP_LOGE(TAG, "Setpoint %f out of range", setpoint);
        return false;
    }
    
    return true;
}
```

## 🔄 Міграція конфігурацій

### При оновленні структури:

1. **Збільшіть номер версії**
```json
{
    "version": 2,  // було 1
    // ...
}
```

2. **Додайте логіку міграції**
```cpp
void ConfigManager::migrate_config(json& config) {
    int version = config.value("version", 1);
    
    if (version == 1) {
        // Міграція з версії 1 до 2
        if (config.contains("old_field")) {
            config["new_field"] = config["old_field"];
            config.erase("old_field");
        }
        config["version"] = 2;
    }
}
```

3. **Документуйте зміни**
```markdown
## Changelog
- v2: Перейменовано 'old_field' → 'new_field'
- v1: Початкова версія
```

## 📍 Розташування файлів

```
components/core/configs/
├── default_config.json      # Стандартний холодильник
├── freezer_config.json      # Морозильна камера
├── ripening_config.json     # Камера дозрівання
├── my_equipment_config.json # Ваша конфігурація
└── README.md               # Ця документація
```

## 🔗 Пов'язана документація

- [ConfigManager API](../03_Components/core/config_manager.md)
- [Архітектура системи](../02_Architecture/system_architecture.md)
- [Керування модулями](../03_Components/core/module_manager.md)

---

*Оновлено для Phase 5 Architecture*
