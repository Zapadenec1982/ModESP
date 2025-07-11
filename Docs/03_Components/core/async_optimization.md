# ConfigManager LittleFS Optimization

## Проблема
Під час тестування ConfigManager виявлено проблему з довгими операціями читання/запису, що призводить до спрацювання watchdog timer. Це відбувається через:
- Синхронні операції з файлами
- Великі JSON об'єкти (кілька модулів по кілька КБ кожен)
- Відсутність годування watchdog під час операцій

## Рішення: Асинхронний ConfigManager

### Ключові особливості:

1. **Асинхронний запис**
   - Окрема задача для операцій з файлами
   - Черга запитів на збереження
   - Групування змін (batching)

2. **Watchdog-friendly**
   - Регулярне годування watchdog під час операцій
   - Розбиття великих файлів на chunks
   - Контроль часу виконання

3. **Оптимізація продуктивності**
   - Групування змін для зменшення кількості записів
   - Кешування в RAM
   - Інкрементальне збереження

### Використання:

```cpp
// Ініціалізація
ConfigManagerAsync::AsyncConfig config;
config.write_queue_size = 20;
config.batch_delay_ms = 200;  // Чекати 200ms для групування
config.watchdog_feed_interval = 30;  // Годувати watchdog кожні 30ms

ConfigManagerAsync::init_async(config);

// Планування збереження окремого модуля
ConfigManager::set("sensors.temperature.offset", 1.5);
ConfigManagerAsync::schedule_save("sensors");  // Неблокуюче

// Планування збереження всієї конфігурації
ConfigManagerAsync::schedule_save_all();

// Примусове збереження перед виключенням
ConfigManagerAsync::flush_pending_saves(3000);  // Timeout 3 секунди

// Отримання статистики
auto stats = ConfigManagerAsync::get_async_stats();
ESP_LOGI(TAG, "Saves: %d completed, %d failed, avg time: %dms",
         stats.completed_saves, stats.failed_saves,
         stats.total_write_time_ms / stats.completed_saves);
```

### Інтеграція з існуючим ConfigManager:

```cpp
// У config_manager.cpp, функція set():
esp_err_t set(const std::string& path, const nlohmann::json& value) {
    // ... існуючий код ...
    
    #ifdef CONFIG_ASYNC_SAVE_ENABLED
    // Замість прямого збереження планувати асинхронне
    std::string module_name = extract_module_name(path);
    ConfigManagerAsync::schedule_save(module_name);
    #endif
    
    return ESP_OK;
}

// В Application::stop():
void Application::stop() {
    // Зберегти всі незбережені зміни
    ConfigManagerAsync::flush_pending_saves(5000);
    ConfigManagerAsync::stop_async();
    // ... інше ...
}
```

### Переваги:

1. **Відсутність блокування**
   - UI залишається responsive
   - Модулі не блокуються на I/O операціях

2. **Надійність**
   - Watchdog не спрацьовує
   - Graceful shutdown з збереженням всіх змін

3. **Ефективність**
   - Менше операцій запису через групування
   - Продовження роботи системи під час збереження

### Налаштування Watchdog:

```cpp
// У main.cpp або application.cpp
esp_task_wdt_config_t wdt_config = {
    .timeout_ms = 5000,  // 5 секунд замість стандартних 3
    .idle_core_mask = 0,
    .trigger_panic = true
};
esp_task_wdt_reconfigure(&wdt_config);

// Додати задачу writer до watchdog
esp_task_wdt_add(state.writer_task);
```

### Структура файлів у LittleFS:

```
/storage/
├── configs/
│   ├── system.json
│   ├── sensors.json
│   ├── actuators.json
│   ├── climate.json
│   ├── network.json
│   ├── version.dat
│   └── backup/
│       ├── system.json.bak
│       └── ...
├── logs/
└── data/
```

### Тестування:

```cpp
void test_async_config_performance() {
    // Створити велику конфігурацію
    for (int i = 0; i < 100; i++) {
        std::string path = "test.module" + std::to_string(i) + ".data";
        nlohmann::json large_data = generate_large_json(10000);  // 10KB
        ConfigManager::set(path, large_data);
    }
    
    // Запланувати збереження
    uint64_t start = esp_timer_get_time();
    ConfigManagerAsync::schedule_save_all();
    
    // Продовжити роботу
    while (ConfigManagerAsync::get_async_stats().pending_saves > 0) {
        // Система продовжує працювати
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    uint64_t duration = esp_timer_get_time() - start;
    ESP_LOGI(TAG, "Async save completed in %lld ms", duration / 1000);
}
```

## Висновок

Асинхронний ConfigManager вирішує проблему watchdog timeout при збереженні великих конфігурацій, забезпечуючи:
- Неблокуючі операції
- Надійне збереження даних
- Оптимальну продуктивність
- Просту інтеграцію з існуючим кодом
