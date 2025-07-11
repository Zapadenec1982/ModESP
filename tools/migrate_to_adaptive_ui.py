#!/usr/bin/env python3
"""
Скрипт для міграції веб-функціоналу з ui/ в adaptive_ui/
"""
import os
import shutil
import re
from datetime import datetime

def create_web_adapter_structure():
    """Створити структуру для веб-адаптера в adaptive_ui"""
    base_path = "components/adaptive_ui/adapters/web"
    
    dirs = [
        f"{base_path}/include",
        f"{base_path}/src"
    ]
    
    for dir_path in dirs:
        os.makedirs(dir_path, exist_ok=True)
        print(f"[+] Created {dir_path}")

def migrate_web_ui_module():
    """Перенести та адаптувати web_ui_module"""
    
    # Шляхи файлів
    old_header = "components/ui/include/web_ui_module.h"
    old_source = "components/ui/src/web_ui_module.cpp"
    new_header = "components/adaptive_ui/adapters/web/include/web_ui_adapter.h"
    new_source = "components/adaptive_ui/adapters/web/src/web_ui_adapter.cpp"
    
    # Копіювати файли
    if os.path.exists(old_header):
        shutil.copy2(old_header, new_header)
        print(f"[+] Copied {old_header} -> {new_header}")
        
        # Адаптувати header
        with open(new_header, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Змінити назву класу та namespace
        content = content.replace('WebUIModule', 'WebUIAdapter')
        content = content.replace('WEB_UI_MODULE_H', 'WEB_UI_ADAPTER_H')
        
        # Додати namespace
        if 'namespace ModESP::UI' not in content:
            content = re.sub(
                r'(class WebUIAdapter)',
                'namespace ModESP::UI {\n\n\\1',
                content
            )
            content = content.rstrip() + '\n\n} // namespace ModESP::UI\n'
        
        with open(new_header, 'w', encoding='utf-8') as f:
            f.write(content)
        print("[+] Adapted header file")
    
    if os.path.exists(old_source):
        shutil.copy2(old_source, new_source)
        print(f"[+] Copied {old_source} -> {new_source}")
        
        # Адаптувати source
        with open(new_source, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Оновити includes та назви
        content = content.replace('"web_ui_module.h"', '"web_ui_adapter.h"')
        content = content.replace('WebUIModule', 'WebUIAdapter')
        
        # Додати namespace
        content = re.sub(
            r'(#include.*\n)+',
            '\\g<0>\nnamespace ModESP::UI {\n',
            content,
            count=1
        )
        content = content.rstrip() + '\n\n} // namespace ModESP::UI\n'
        
        with open(new_source, 'w', encoding='utf-8') as f:
            f.write(content)
        print("[+] Adapted source file")

def migrate_api_dispatcher():
    """Перенести api_dispatcher як api_handler"""
    
    old_header = "components/ui/include/api_dispatcher.h"
    old_source = "components/ui/src/api_dispatcher.cpp"
    new_header = "components/adaptive_ui/adapters/web/include/api_handler.h"
    new_source = "components/adaptive_ui/adapters/web/src/api_handler.cpp"
    
    if os.path.exists(old_header):
        shutil.copy2(old_header, new_header)
        print(f"[+] Copied {old_header} -> {new_header}")
        
        # Адаптувати
        with open(new_header, 'r', encoding='utf-8') as f:
            content = f.read()
        
        content = content.replace('ApiDispatcher', 'ApiHandler')
        content = content.replace('API_DISPATCHER_H', 'API_HANDLER_H')
        
        # Додати namespace
        if 'namespace ModESP::UI' not in content:
            content = re.sub(
                r'(class ApiHandler)',
                'namespace ModESP::UI {\n\n\\1',
                content
            )
            content = content.rstrip() + '\n\n} // namespace ModESP::UI\n'
        
        with open(new_header, 'w', encoding='utf-8') as f:
            f.write(content)
    
    if os.path.exists(old_source):
        shutil.copy2(old_source, new_source)
        print(f"[+] Copied {old_source} -> {new_source}")
        
        # Адаптувати source
        with open(new_source, 'r', encoding='utf-8') as f:
            content = f.read()
        
        content = content.replace('"api_dispatcher.h"', '"api_handler.h"')
        content = content.replace('ApiDispatcher', 'ApiHandler')
        
        # Додати namespace
        content = re.sub(
            r'(#include.*\n)+',
            '\\g<0>\nnamespace ModESP::UI {\n',
            content,
            count=1
        )
        content = content.rstrip() + '\n\n} // namespace ModESP::UI\n'
        
        with open(new_source, 'w', encoding='utf-8') as f:
            f.write(content)

def update_cmake():
    """Оновити CMakeLists.txt"""
    
    cmake_file = "components/adaptive_ui/CMakeLists.txt"
    
    if os.path.exists(cmake_file):
        with open(cmake_file, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Додати нові файли до SRCS
        new_srcs = '''        "adapters/web/src/web_ui_adapter.cpp"
        "adapters/web/src/api_handler.cpp"'''
        
        content = re.sub(
            r'(SRCS\s*\n)(.*?)(\n\s*INCLUDE_DIRS)',
            f'\\1\\2\n{new_srcs}\\3',
            content,
            flags=re.DOTALL
        )
        
        # Додати include dirs
        content = re.sub(
            r'(INCLUDE_DIRS.*?)(\n\s*REQUIRES)',
            '\\1\n        "adapters/web/include"\\2',
            content,
            flags=re.DOTALL
        )
        
        # Додати esp_http_server до REQUIRES
        if 'esp_http_server' not in content:
            content = re.sub(
                r'(REQUIRES\s*\n)(.*?)(\n\s*PRIV_REQUIRES)',
                '\\1\\2\n        esp_http_server\\3',
                content,
                flags=re.DOTALL
            )
        
        with open(cmake_file, 'w', encoding='utf-8') as f:
            f.write(content)
        print("[+] Updated CMakeLists.txt")

def create_integration_example():
    """Створити приклад інтеграції"""
    
    example = '''// Приклад використання WebUIAdapter з adaptive_ui
#include "web_ui_adapter.h"
#include "ui_filter.h"
#include "lazy_component_loader.h"

using namespace ModESP::UI;

void setup_web_ui() {
    // Ініціалізація фільтра
    UIFilter filter;
    filter.setConfig(system_config);
    filter.setUserRole(UserRole::TECHNICIAN);
    
    // Ініціалізація loader
    LazyComponentLoader& loader = LazyLoaderManager::getInstance();
    
    // Створити та запустити веб-адаптер
    auto web_adapter = std::make_unique<WebUIAdapter>(&filter, &loader);
    
    if (web_adapter->start(80) == ESP_OK) {
        ESP_LOGI("WebUI", "Web interface started on port 80");
    }
}
'''
    
    example_file = "components/adaptive_ui/examples/web_ui_example.cpp"
    os.makedirs(os.path.dirname(example_file), exist_ok=True)
    
    with open(example_file, 'w', encoding='utf-8') as f:
        f.write(example)
    print(f"[+] Created example: {example_file}")

def main():
    print("Starting migration from ui/ to adaptive_ui/")
    print("=" * 50)
    
    # Переходимо в правильну директорію
    os.chdir("C:/ModESP_dev")
    
    # Резервне копіювання
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_dir = f"components/_archive_ui/ui_backup_{timestamp}"
    
    if os.path.exists("components/ui"):
        os.makedirs(os.path.dirname(backup_dir), exist_ok=True)
        shutil.copytree("components/ui", backup_dir)
        print(f"[+] Backup created: {backup_dir}")
    
    # Виконати міграцію
    create_web_adapter_structure()
    migrate_web_ui_module()
    migrate_api_dispatcher()
    update_cmake()
    create_integration_example()
    
    print("\n[OK] Migration completed!")
    print("\nNext steps:")
    print("1. Review and adapt the migrated code")
    print("2. Update core/CMakeLists.txt to use adaptive_ui instead of ui")
    print("3. Test the web interface")
    print("4. Remove ui/ component after successful testing")

if __name__ == "__main__":
    main()
