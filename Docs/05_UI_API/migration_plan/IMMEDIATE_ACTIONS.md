# Негайні дії для впорядкування UI компонентів

## 🚀 Кроки для виконання зараз

### 1. Створення структури unified_ui (5 хвилин)

```bash
# Виконати в корені проекту
mkdir -p components/unified_ui/{include/{core,adapters,components,utils},src/{core,adapters,components,utils}}
mkdir -p components/_archive_ui/{old_ui,adaptive_drafts}
```

### 2. Створення базових інтерфейсів (10 хвилин)

**Файл: `components/unified_ui/include/core/ui_types.h`**
```cpp
#pragma once

namespace ModESP::UI {
    // Об'єднаний enum типів компонентів
    enum class ComponentType {
        VALUE,      // Read-only відображення  
        INPUT,      // Number/text input
        TOGGLE,     // Boolean switch
        BUTTON,     // Action button
        SLIDER,     // Range control
        SELECT,     // Dropdown selection
        CHART,      // Data visualization
        LIST,       // Item list
        COMPOSITE   // Container
    };
    
    // Рівні доступу з adaptive_ui
    enum class AccessLevel {
        USER = 0,
        OPERATOR = 1,
        TECHNICIAN = 2,
        SUPERVISOR = 3,
        ADMIN = 4
    };
    
    // Пріоритет завантаження
    enum class Priority {
        LOW,
        MEDIUM,
        HIGH,
        CRITICAL
    };
}
```

### 3. Базовий CMakeLists.txt (5 хвилин)

**Файл: `components/unified_ui/CMakeLists.txt`**
```cmake
idf_component_register(
    SRCS 
        # Поки що порожньо, додамо при міграції
    INCLUDE_DIRS 
        "include"
    REQUIRES 
        core
        base_module
        esp_http_server
        nvs_flash
        esp_wifi
        mittelab__nlohmann-json
    PRIV_REQUIRES
        mbedtls
        esp_timer
        esp_log
)
```

### 4. Скрипт для безпечної міграції (15 хвилин)

**Файл: `tools/migrate_ui_safe.py`**
```python
#!/usr/bin/env python3
"""
Безпечна міграція UI компонентів з резервним копіюванням
"""
import os
import shutil
from datetime import datetime

def backup_and_migrate():
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    
    # 1. Створити резервні копії
    print(f"Creating backup with timestamp: {timestamp}")
    
    backup_dir = f"components/_archive_ui/backup_{timestamp}"
    os.makedirs(backup_dir, exist_ok=True)
    
    # Копіювати існуючі папки
    if os.path.exists("components/ui"):
        shutil.copytree("components/ui", f"{backup_dir}/ui")
        print("✓ Backed up components/ui")
        
    if os.path.exists("components/adaptive_ui"):
        shutil.copytree("components/adaptive_ui", f"{backup_dir}/adaptive_ui")
        print("✓ Backed up components/adaptive_ui")
    
    # 2. Створити мапу файлів для міграції
    migration_map = {
        # З ui/
        "components/ui/include/web_ui_module.h": 
            "components/unified_ui/include/adapters/web/web_ui_adapter.h",
        "components/ui/src/web_ui_module.cpp": 
            "components/unified_ui/src/adapters/web/web_ui_adapter.cpp",
        "components/ui/include/api_dispatcher.h": 
            "components/unified_ui/include/core/api_dispatcher.h",
        "components/ui/src/api_dispatcher.cpp": 
            "components/unified_ui/src/core/api_dispatcher.cpp",
            
        # З adaptive_ui/
        "components/adaptive_ui/include/ui_filter.h": 
            "components/unified_ui/include/core/ui_filter.h",
        "components/adaptive_ui/ui_filter.cpp": 
            "components/unified_ui/src/core/ui_filter.cpp",
        "components/adaptive_ui/include/lazy_component_loader.h": 
            "components/unified_ui/include/core/component_loader.h",
        "components/adaptive_ui/lazy_component_loader.cpp": 
            "components/unified_ui/src/core/component_loader.cpp",
    }
    
    # 3. Копіювати файли в нову структуру
    print("\nMigrating files to unified_ui:")
    for src, dst in migration_map.items():
        if os.path.exists(src):
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            print(f"✓ {src} → {dst}")
    
    print("\n✅ Migration completed! Original files are backed up.")
    print(f"📁 Backup location: {backup_dir}")

if __name__ == "__main__":
    backup_and_migrate()
```

### 5. Оновлення includes (10 хвилин)

**Скрипт: `tools/update_ui_includes.py`**
```python
#!/usr/bin/env python3
"""
Оновлення include шляхів для нової структури
"""
import os
import re

# Мапа старих та нових include
include_map = {
    '"web_ui_module.h"': '"unified_ui/adapters/web/web_ui_adapter.h"',
    '"ui_adapter_base.h"': '"unified_ui/core/base_adapter.h"',
    '"api_dispatcher.h"': '"unified_ui/core/api_dispatcher.h"',
    '"ui_filter.h"': '"unified_ui/core/ui_filter.h"',
    '"lazy_component_loader.h"': '"unified_ui/core/component_loader.h"',
    '"ui_component_base.h"': '"unified_ui/core/component_interface.h"',
}

def update_includes(root_dir):
    for root, dirs, files in os.walk(root_dir):
        # Пропустити архівні папки
        if '_archive' in root:
            continue
            
        for file in files:
            if file.endswith(('.h', '.cpp', '.c')):
                filepath = os.path.join(root, file)
                
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                modified = False
                for old, new in include_map.items():
                    if old in content:
                        content = content.replace(old, new)
                        modified = True
                        print(f"✓ Updated {filepath}: {old} → {new}")
                
                if modified:
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(content)

if __name__ == "__main__":
    update_includes("C:/ModESP_dev")
```

## ⚡ Швидкі перемоги (можна зробити зараз)

### 1. Видалити невикористані файли (2 хвилини)
```bash
# В components/adaptive_ui/
rm include/base_driver.h  # Дублює BaseModule
```

### 2. Створити unified enum (5 хвилин)
Створити файл `components/unified_ui/include/core/ui_types.h` з кодом вище.

### 3. Оновити документацію (5 хвилин)
Додати в `Docs/05_UI_API/README.md`:
```markdown
## 🔄 Статус міграції

**Активна робота**: Об'єднання `ui/` та `adaptive_ui/` в `unified_ui/`

- Детальний план: [UI_CONSOLIDATION_PLAN.md](migration_plan/UI_CONSOLIDATION_PLAN.md)
- Аналіз дублювання: [CODE_DUPLICATION_ANALYSIS.md](migration_plan/CODE_DUPLICATION_ANALYSIS.md)
```

## 📋 Чек-лист негайних дій

- [ ] Створити структуру папок unified_ui
- [ ] Створити базові header файли
- [ ] Запустити скрипт резервного копіювання
- [ ] Видалити base_driver.h
- [ ] Оновити README з статусом міграції
- [ ] Створити початковий CMakeLists.txt

## 🎯 Наступні кроки (після негайних дій)

1. Портувати WebUIModule як WebUIAdapter
2. Інтегрувати UIFilter в базовий адаптер
3. Об'єднати LCD адаптери
4. Оновити генератор для нової структури
5. Провести тестування

## ⚠️ Важливо

- **НЕ видаляти** оригінальні файли до повного тестування
- **Зберігати** резервні копії мінімум 2 тижні
- **Тестувати** кожен крок міграції
- **Документувати** всі зміни
