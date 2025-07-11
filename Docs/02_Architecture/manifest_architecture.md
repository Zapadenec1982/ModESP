# ModESP Modular Manifest-Driven Architecture

## Концепція
Модульна архітектура з self-describing модулями через маніфести. Кожен модуль містить `module_manifest.json` з повним описом своїх можливостей, API, UI та конфігурації.

## Ключові принципи
1. **Self-describing modules** - модулі описують себе через маніфести
2. **Build-time optimization** - генерація коду під час компіляції
3. **Runtime flexibility** - динамічна адаптація UI та API
4. **Role-based access** - різні рівні доступу для різних користувачів
5. **Multi-channel support** - єдина архітектура для всіх каналів комунікації

## Основні компоненти

### 1. Module Manifest
- Повний опис модуля в JSON форматі
- Містить: metadata, APIs, UI schemas, configuration, dependencies
- Використовується для генерації коду та runtime конфігурації

### 2. Build System
- `process_manifests.py` - обробляє всі маніфести
- Генерує: API registry, Module registry, UI resources
- Інтегрується з ESP-IDF build system

### 3. Runtime System
- ModuleManager - керує життєвим циклом модулів
- ApiDispatcher - маршрутизує API виклики
- DynamicMenuBuilder - будує адаптивні меню
- AccessController - контролює доступ

### 4. Communication Channels
- LCD Menu - локальний інтерфейс
- Web UI - веб-інтерфейс (AP/STA)
- MQTT - інтеграція з IoT
- Telegram Bot - повідомлення та керування
- Mobile App - нативний додаток
- Modbus RTU - промислова інтеграція

## Потік даних
1. Manifest → Build System → Generated Code
2. Configuration → Runtime System → Dynamic UI
3. User Request → Access Control → Module API → Response

## Рівні доступу
- **User** - базові функції перегляду
- **Technician** - налаштування та калібрування
- **Administrator** - повний доступ до конфігурації
- **Supervisor** - критичні операції та factory reset