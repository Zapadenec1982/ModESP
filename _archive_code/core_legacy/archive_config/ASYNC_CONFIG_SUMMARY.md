# ConfigManager LittleFS з асинхронним збереженням

## Підсумок оптимізації

Проблема watchdog timeout під час збереження великих конфігурацій вирішена через впровадження асинхронного менеджера збереження.

### Основні переваги:

1. **Watchdog-safe операції**
   - Регулярне годування watchdog під час запису
   - Розбиття великих файлів на chunks
   - Неблокуюче збереження

2. **Покращена продуктивність**
   - Групування змін (batching) зменшує кількість записів
   - Асинхронні операції не блокують основний цикл
   - Паралельна робота з іншими модулями

3. **Проста інтеграція**
   - Мінімальні зміни в існуючому коді
   - Налаштування через menuconfig
   - Автоматичне збереження змін (опційно)

### Використання:

```cpp
// Включити в menuconfig:
// Component config → Configuration Manager Options → 
//   [*] Enable asynchronous configuration save

// Або в sdkconfig:
CONFIG_USE_ASYNC_SAVE=y
CONFIG_ASYNC_SAVE_QUEUE_SIZE=20
CONFIG_ASYNC_SAVE_BATCH_DELAY_MS=200
CONFIG_ASYNC_SAVE_WATCHDOG_FEED_MS=30
```

### Тестування:

```bash
# Запустити тести
cd test
build_and_run.bat

# Дивитися на тест ConfigManager Async
# Перевірити, що великі конфігурації зберігаються без watchdog timeout
```

### Структура файлів:

```
components/core/
├── config_manager.cpp         # Основний менеджер (модифікований)
├── config_manager_async.h     # Асинхронний інтерфейс
├── config_manager_async.cpp   # Асинхронна реалізація
├── Kconfig.projbuild         # Налаштування
└── CONFIG_ASYNC_OPTIMIZATION.md # Документація

test/target/
└── test_config_async.cpp     # Тести асинхронного менеджера
```

### Результати:

- ✅ Watchdog більше не спрацьовує при збереженні
- ✅ Система залишається responsive під час запису
- ✅ Зменшено кількість операцій запису через batching
- ✅ Graceful shutdown з гарантованим збереженням

### Наступні кроки:

1. Включити асинхронне збереження в production
2. Налаштувати параметри під конкретне обладнання
3. Моніторити статистику збереження
4. Розглянути compression для ще більшої оптимізації
