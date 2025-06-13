# Рекомендації для ESP32 з PSRAM (2MB)

## Що МОЖНА розмістити в PSRAM:
- Великі буфери даних (JSON, логи, історія)
- Кеші зображень/шрифтів для дисплея
- Буфери мережевих пакетів
- Історію подій/помилок (ErrorCollector)
- Конфігураційні дані

## Що НЕ МОЖНА розмістити в PSRAM:
- C++ exception emergency pool
- Stack для FreeRTOS задач
- DMA буфери
- Код ISR (переривань)
- Критичні структури даних реального часу

## Приклад використання PSRAM ефективно:

```cpp
// В menuconfig включити PSRAM:
// Component config → ESP32-specific → Support for external, SPI-connected RAM

// Перевірка наявності PSRAM
void app_main() {
    ESP_LOGI(TAG, "Free PSRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Free SRAM: %d bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

// Розумне використання пам'яті
class DataLogger {
private:
    // Критичні дані - в SRAM
    QueueHandle_t m_queue;           // SRAM
    SemaphoreHandle_t m_mutex;       // SRAM
    
    // Великі буфери - в PSRAM
    char* m_log_buffer;              // PSRAM
    ErrorInfo* m_error_history;      // PSRAM
    
public:
    DataLogger() {
        // Критичні структури в SRAM
        m_queue = xQueueCreate(10, sizeof(LogEntry*));
        m_mutex = xSemaphoreCreateMutex();
        
        // Буфери в PSRAM
        m_log_buffer = (char*)heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);
        m_error_history = (ErrorInfo*)heap_caps_malloc(
            sizeof(ErrorInfo) * 1000, MALLOC_CAP_SPIRAM);
    }
};
```

## Продуктивність з PSRAM:

| Операція | SRAM | PSRAM | Різниця |
|----------|------|-------|---------|
| Read 4 bytes | 1 цикл | 8-10 циклів | x8-10 |
| Read 1KB sequential | ~250 циклів | ~2000 циклів | x8 |
| Random access | 1 цикл | 10-40 циклів | x10-40 |
| DMA transfer | Працює | НЕ працює | - |

## ВИСНОВОК для вашого проекту:

1. **PSRAM допомагає**, але НЕ для винятків C++
2. **ErrorCollector в 36 разів ефективніший** за винятки
3. **З PSRAM можна**:
   - Зберігати 1000+ помилок замість 10
   - Мати детальну історію подій
   - Великі JSON конфігурації
   
4. **Рекомендую**:
   - Відключити винятки C++
   - Використовувати коди помилок + ErrorCollector
   - PSRAM для історії даних та буферів
   - SRAM для критичного коду

Це дасть найкращий баланс між функціональністю та продуктивністю!
