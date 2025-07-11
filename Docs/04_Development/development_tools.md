# 🛠️ Інструменти розробки ModESP

## 📋 Огляд

Папка `tools/` містить скрипти та утиліти для автоматизації розробки ModESP, включаючи генератори коду, валідатори та допоміжні інструменти.

## 📁 Структура tools/

```
tools/
├── adaptive_ui_generator.py    # Генератор Adaptive UI компонентів
├── process_manifests.py        # Основний обробник маніфестів
├── ui_generator.py            # Генератор UI (Phase 2)
├── event_code_generator.py    # Генератор подій
├── module_info.py             # Утиліти для модулів
├── test_generator.py          # Генератор тестів
├── test_phase5.sh            # Тестовий скрипт Phase 5
├── cleanup_phase2.sh         # Очищення Phase 2
├── manifest_schemas/         # JSON схеми для валідації
├── manifest_tools/           # Допоміжні інструменти
└── __pycache__/             # Python кеш (генерується автоматично)
```

## 🚀 Основні інструменти

### 1. **process_manifests.py** - Головний інструмент

**Призначення**: Обробка маніфестів модулів та генерація коду

**Використання**:
```bash
# Базове використання
python tools/process_manifests.py --project-root . --output-dir main/generated

# З детальним логуванням
python tools/process_manifests.py --project-root . --output-dir main/generated --verbose

# Тільки валідація без генерації
python tools/process_manifests.py --project-root . --validate-only
```

**Функції**:
- Сканування проекту на маніфести
- Валідація структури маніфестів
- Генерація UI компонентів
- Створення реєстрів модулів
- Інтеграція з CMake

### 2. **adaptive_ui_generator.py** - Генератор Adaptive UI

**Призначення**: Генерація компонентів для Phase 5 Adaptive UI Architecture

**Функції**:
- Генерація UI компонентів з маніфестів
- Створення рендерів для різних каналів (LCD, Web, MQTT)
- Умовна фільтрація компонентів
- Ледача завантаження (lazy loading)

**Інтеграція**:
```python
from adaptive_ui_generator import generate_adaptive_ui

# У process_manifests.py
generate_adaptive_ui(modules, drivers, output_dir)
```

### 3. **ui_generator.py** - Генератор UI (застарілий)

**Призначення**: Генерація UI для попередньої архітектури

**Статус**: Застарілий, рекомендується використовувати Adaptive UI

### 4. **event_code_generator.py** - Генератор подій

**Призначення**: Автоматична генерація коду для системи подій

**Функції**:
- Генерація типів подій
- Створення обробників
- Валідація схем подій

## 🔧 Допоміжні інструменти

### manifest_schemas/ - JSON схеми

**Призначення**: Валідація структури маніфестів

**Файли**:
- `module_manifest.schema.json` - Схема маніфесту модуля
- `driver_manifest.schema.json` - Схема маніфесту драйвера
- `ui_component.schema.json` - Схема UI компонента

### manifest_tools/ - Утиліти маніфестів

**Призначення**: Допоміжні інструменти для роботи з маніфестами

**Інструменти**:
- Валідатори
- Конвертери
- Аналізатори залежностей

## 📝 Скрипти автоматизації

### test_phase5.sh - Тестування Adaptive UI

**Призначення**: Автоматизоване тестування сучасної архітектури

```bash
# Запуск тестів
chmod +x tools/test_phase5.sh
./tools/test_phase5.sh
```

**Перевіряє**:
- Валідність маніфестів
- Генерацію коду
- Компіляцію проекту
- Функціональні тести

### cleanup_legacy.sh - Очищення застарілих файлів

**Призначення**: Видалення застарілих файлів попередніх архітектур

```bash
# Очищення (УВАГА: незворотно!)
chmod +x tools/cleanup_phase2.sh
./tools/cleanup_phase2.sh
```

## 🔍 Використання в розробці

### Типовий workflow:

1. **Створення маніфесту**:
```json
// У вашому модулі
{
  "module": {
    "name": "MyModule",
    "type": "SENSOR_MANAGER"
  },
  "ui": {
    "adaptive": {
      "components": [...]
    }
  }
}
```

2. **Генерація коду**:
```bash
python tools/process_manifests.py --project-root . --output-dir main/generated
```

3. **Збірка проекту**:
```bash
idf.py build
```

### Інтеграція з CMake:

```cmake
# У CMakeLists.txt
execute_process(
    COMMAND python ${CMAKE_SOURCE_DIR}/tools/process_manifests.py 
            --project-root ${CMAKE_SOURCE_DIR}
            --output-dir ${CMAKE_BINARY_DIR}/generated
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
```

## 🐛 Налагодження та діагностика

### Перевірка маніфестів:
```bash
# Валідація всіх маніфестів
python tools/process_manifests.py --validate-only --verbose

# Перевірка конкретного маніфесту
python -c "
import json
from jsonschema import validate
schema = json.load(open('tools/manifest_schemas/module_manifest.schema.json'))
manifest = json.load(open('components/my_module/module_manifest.json'))
validate(manifest, schema)
print('Маніфест валідний!')
"
```

### Перевірка генерації:
```bash
# Генерація з детальним логуванням
python tools/process_manifests.py --project-root . --output-dir /tmp/test_gen --verbose

# Перевірка згенерованих файлів
ls -la /tmp/test_gen/
```

## 📚 Створення власних інструментів

### Базовий шаблон:

```python
#!/usr/bin/env python3
"""
Мій інструмент для ModESP
"""

import argparse
import json
import logging
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description='Мій інструмент')
    parser.add_argument('--project-root', required=True, help='Корінь проекту')
    parser.add_argument('--verbose', action='store_true', help='Детальний вивід')
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.basicConfig(level=logging.DEBUG)
    
    # Ваша логіка тут
    
if __name__ == '__main__':
    main()
```

### Рекомендації:
- Використовуйте argparse для аргументів командного рядка
- Додавайте логування для налагодження
- Валідуйте вхідні дані
- Створюйте backup файлів перед модифікацією
- Документуйте свої інструменти

## 🔗 Пов'язана документація

- [Adaptive UI Architecture](../module_manifest_architecture/ADAPTIVE_UI_ARCHITECTURE.md)
- [Система маніфестів](../02_Architecture/system_architecture.md)
- [Стандарти розробки](../04_Development/coding_standards.md)
- [Інструкції збірки](../00_Build_Instructions/BUILD_INSTRUCTIONS.md)

---

*Оновлено для Phase 5 Architecture*
