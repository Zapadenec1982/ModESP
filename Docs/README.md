# Документація ModuChill

## Навігація по документації

### 📋 Початок роботи
- [README.md](../README.md) - Головний опис проекту та швидкий старт
- [GETTING_STARTED.md](GETTING_STARTED.md) - Детальний guide для розробників
- [QUICK_START_CHECKLIST.md](QUICK_START_CHECKLIST.md) - 15-хвилинний setup checklist
- [SYSTEM_ARCHITECTURE.md](SYSTEM_ARCHITECTURE.md) - Повна архітектура системи
- [HAL_DEVELOPMENT_SUMMARY.md](HAL_DEVELOPMENT_SUMMARY.md) - Підсумок розробки HAL

### 📈 Планування та розробка
- [ACTION_PLAN.md](ACTION_PLAN.md) - Конкретні наступні кроки та план дій
- [PROJECT_ROADMAP.md](PROJECT_ROADMAP.md) - Roadmap розвитку проекту
- [COLLABORATIVE_ROADMAP.md](COLLABORATIVE_ROADMAP.md) - AI-Human collaboration strategy
- [TODO.md](TODO.md) - Детальний список завдань з критеріями готовності
- [CODING_SESSIONS.md](CODING_SESSIONS.md) - План сесій програмування

### 🛠️ Стандарти розробки
- [DEVELOPMENT_GUIDELINES.md](DEVELOPMENT_GUIDELINES.md) - Coding standards та best practices
- [GIT_WORKFLOW.md](GIT_WORKFLOW.md) - Git workflow та code review процес
- [GETTING_STARTED.md](GETTING_STARTED.md) - Getting started guide для нових розробників

### 🏗️ Архітектура та основні компоненти

#### Core система
- [Core.txt](Core.txt) - Повний довідник по Core модулю
- [Application.txt](Application.txt) - Lifecycle координатор системи
- [SharedState.txt](SharedState.txt) - Централізований обмін даними
- [ConfigManager.txt](ConfigManager.txt) - Система конфігурації
- [ModuleManager.txt](ModuleManager.txt) - Управління модулями

#### Hardware Abstraction Layer (HAL)
- [HAL - Модульна архітектура.txt](HAL%20-%20Модульна%20архітектура.txt) - Концепція модульного HAL
- [HAL - Довідник.txt](HAL%20-%20Довідник.txt) - Повний довідник по HAL інтерфейсам

### 🔧 Драйвери та модулі

#### Датчики (Sensors)
- [SensorModule.txt](SensorModule.txt) - Архітектура модуля датчиків
- [SensorModule_Async_Optimization.md](SensorModule_Async_Optimization.md) - Асинхронна оптимізація
- [DS18B20_ASYNC_SUMMARY.md](DS18B20_ASYNC_SUMMARY.md) - Асинхронний DS18B20 драйвер

#### Актуатори (Actuators)
- [ActuatorModule.txt](ActuatorModule.txt) - Архітектура модуля актуаторів
- [RelayModule.txt](RelayModule.txt) - Драйвер реле з захистом

### ⏰ Спеціальні модулі

#### RTC (Real Time Clock)
- [RTCModule.md](RTCModule.md) - Основна документація RTC модуля
- [RTC_IMPLEMENTATION_SUMMARY.md](RTC_IMPLEMENTATION_SUMMARY.md) - Підсумок реалізації
- [RTC_Integration_Examples.md](RTC_Integration_Examples.md) - Приклади інтеграції

### 🛠️ Технічні зміни та оновлення

#### Оновлення плат
- [BOARD_UPDATE.md](BOARD_UPDATE.md) - Процедура оновлення конфігурації плат
- [BOARD_CHANGES_SUMMARY.md](BOARD_CHANGES_SUMMARY.md) - Підсумок змін у платах
- [CUSTOM_BOARD_CONFIG.md](CUSTOM_BOARD_CONFIG.md) - Конфігурація користувацьких плат

#### Технічні зміни
- [NO_EXCEPTIONS_CHANGES.md](NO_EXCEPTIONS_CHANGES.md) - Видалення C++ exceptions

### 📖 Швидкі довідники

#### По функціональності
| Функція | Документ | Опис |
|---------|----------|------|
| Система ядра | [Core.txt](Core.txt) | EventBus, SharedState, Config |
| Робота з датчиками | [SensorModule.txt](SensorModule.txt) | Додавання та конфігурація датчиків |
| Керування актуаторами | [ActuatorModule.txt](ActuatorModule.txt) | Релейні та PWM драйвери |
| Час та календар | [RTCModule.md](RTCModule.md) | RTC функціональність |
| HAL інтерфейси | [HAL - Довідник.txt](HAL%20-%20Довідник.txt) | Низькорівневі інтерфейси |
| Планування розробки | [PROJECT_ROADMAP.md](PROJECT_ROADMAP.md) | Стратегія та цілі проекту |
| Поточні завдання | [TODO.md](TODO.md) | Actionable development items |
| AI-Human collaboration | [COLLABORATIVE_ROADMAP.md](COLLABORATIVE_ROADMAP.md) | AI розробка стратегія |

#### По процесах розробки
| Процес | Документ | Опис |
|---------|----------|------|
| Quick Start | [QUICK_START_CHECKLIST.md](QUICK_START_CHECKLIST.md) | 15-хвилинний setup та перші кроки |
| Getting Started | [GETTING_STARTED.md](GETTING_STARTED.md) | Повний guide для розробників |
| Action Plan | [ACTION_PLAN.md](ACTION_PLAN.md) | Конкретні наступні кроки |
| Coding Standards | [DEVELOPMENT_GUIDELINES.md](DEVELOPMENT_GUIDELINES.md) | Стандарти коду та best practices |
| Git Workflow | [GIT_WORKFLOW.md](GIT_WORKFLOW.md) | Робочий процес з Git та code review |
| Coding Sessions | [CODING_SESSIONS.md](CODING_SESSIONS.md) | Планування сесій програмування |
| Documentation Summary | [DOCUMENTATION_SUMMARY.md](DOCUMENTATION_SUMMARY.md) | Підсумок систематизації документації |

#### По компонентах
| Компонент | Статус | Документація |
|-----------|---------|--------------|
| Core | ✅ Завершено | [Core.txt](Core.txt) |
| SensorModule | ✅ Завершено | [SensorModule.txt](SensorModule.txt) |
| ActuatorModule | ✅ Завершено | [ActuatorModule.txt](ActuatorModule.txt) |
| RTCModule | ✅ Завершено | [RTCModule.md](RTCModule.md) |
| HAL | ✅ Завершено | [HAL - Довідник.txt](HAL%20-%20Довідник.txt) |
| UIModule | 🚧 В розробці | - |
| NetworkModule | 🚧 В розробці | - |

## Робочі процеси розробки

### Додавання нового драйвера датчика
1. Прочитайте [SensorModule.txt](SensorModule.txt) розділ "Додавання нового драйвера"
2. Подивіться приклади у [SensorModule_Async_Optimization.md](SensorModule_Async_Optimization.md)
3. Додайте драйвер у відповідну папку `components/sensor_drivers/`

### Додавання нового драйвера актуатора
1. Прочитайте [ActuatorModule.txt](ActuatorModule.txt) розділ "Створення драйвера"
2. Використайте [RelayModule.txt](RelayModule.txt) як приклад
3. Додайте драйвер у папку `components/actuator_drivers/`

### Конфігурація нової плати
1. Ознайомтесь з [CUSTOM_BOARD_CONFIG.md](CUSTOM_BOARD_CONFIG.md)
2. Використайте [BOARD_UPDATE.md](BOARD_UPDATE.md) для процедури
3. Перегляньте [BOARD_CHANGES_SUMMARY.md](BOARD_CHANGES_SUMMARY.md) для контексту

### Інтеграція RTC функцій
1. Прочитайте [RTCModule.md](RTCModule.md) для основ
2. Використайте приклади з [RTC_Integration_Examples.md](RTC_Integration_Examples.md)
3. Перегляньте [RTC_IMPLEMENTATION_SUMMARY.md](RTC_IMPLEMENTATION_SUMMARY.md) для деталей

## Оновлення документації

### Актуальність документів

#### ✅ Актуальні документи
- SYSTEM_ARCHITECTURE.md - відповідає поточній архітектурі
- HAL_DEVELOPMENT_SUMMARY.md - останні зміни в HAL
- Core.txt - відповідає поточній реалізації
- SensorModule.txt - актуальна модульна система
- ActuatorModule.txt - актуальна реалізація
- PROJECT_ROADMAP.md - стратегічний план розвитку
- TODO.md - актуальні завдання розробки
- ACTION_PLAN.md - конкретні наступні кроки
- DEVELOPMENT_GUIDELINES.md - стандарти коду
- GIT_WORKFLOW.md - процеси Git та code review
- GETTING_STARTED.md - guide для нових розробників
- QUICK_START_CHECKLIST.md - швидкий setup checklist
- CODING_SESSIONS.md - план сесій програмування
- COLLABORATIVE_ROADMAP.md - AI-Human collaboration стратегія
- DOCUMENTATION_SUMMARY.md - підсумок систематизації

#### ⚠️ Потребують оновлення
- HAL - Довідник.txt - потребує синхронізації з останніми змінами
- ConfigManager.txt - додати нові можливості
- ModuleManager.txt - оновити API

#### 🔄 Планується створити
- NetworkModule.txt - документація мережевих функцій
- UIModule.txt - документація інтерфейсу користувача
- DeploymentGuide.md - керівництво по розгортанню
- TroubleshootingGuide.md - вирішення проблем

## Стиль документації

### Структура документа
1. **Огляд** - коротке пояснення призначення
2. **Архітектура** - структура та компоненти
3. **API довідник** - методи та інтерфейси
4. **Приклади використання** - практичні кейси
5. **Конфігурація** - налаштування та параметри
6. **Найкращі практики** - рекомендації розробникам

### Рекомендації
- Використовуйте markdown для нових документів
- Включайте діаграми ASCII для архітектури
- Додавайте приклади коду
- Підтримуйте зворотні посилання між документами
- Оновлюйте цей індекс при додаванні нових документів

---

**Примітка**: Цей індекс автоматично генерується та підтримується. Останнє оновлення: [дата генерації]