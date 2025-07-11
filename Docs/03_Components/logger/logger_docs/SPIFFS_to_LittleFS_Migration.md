# Міграція з SPIFFS на LittleFS - Короткий посібник

## Швидка інструкція

### 1. Оновити partitions.csv
```diff
- storage,  data, spiffs,  0x210000, 0x100000,
+ storage,  data, 0x82,    0x210000, 0x100000,
```

### 2. Оновити залежності в CMakeLists.txt
```diff
PRIV_REQUIRES
-    spiffs
+    esp_littlefs
```

### 3. Оновити include файли
```diff
- #include <esp_spiffs.h>
+ #include <esp_littlefs.h>
```

### 4. Замінити API виклики

#### Реєстрація файлової системи
```diff
- esp_vfs_spiffs_conf_t conf = {
-     .base_path = "/storage",
-     .partition_label = "storage",
-     .max_files = 10,
-     .format_if_mount_failed = true
- };
- esp_err_t ret = esp_vfs_spiffs_register(&conf);

+ esp_vfs_littlefs_conf_t conf = {
+     .base_path = "/storage",
+     .partition_label = "storage",
+     .format_if_mount_failed = true,
+     .dont_mount = false
+ };
+ esp_err_t ret = esp_vfs_littlefs_register(&conf);
```

#### Отримання інформації про FS
```diff
- esp_spiffs_info("storage", &total, &used);
+ esp_littlefs_info("storage", &total, &used);
```

#### Видалення реєстрації
```diff
- esp_vfs_spiffs_unregister("storage");
+ esp_vfs_littlefs_unregister("storage");
```

#### Форматування
```diff
- esp_spiffs_format("storage");
+ esp_littlefs_format("storage");
```

### 5. Оновити sdkconfig.defaults
```bash
# Видалити SPIFFS конфігурацію
# CONFIG_SPIFFS_* 

# Додати LittleFS конфігурацію
CONFIG_LITTLEFS_MAX_PARTITIONS=1
CONFIG_LITTLEFS_PAGE_SIZE=256
CONFIG_LITTLEFS_CACHE_SIZE=512
CONFIG_LITTLEFS_USE_MTIME=y
```

### 6. Очистити та перекомпілювати
```bash
idf.py fullclean
idf.py build
```

## Основні відмінності

| SPIFFS | LittleFS |
|--------|----------|
| max_files потрібен | max_files не потрібен |
| Немає справжніх директорій | Повна підтримка директорій |
| Статичний wear leveling | Динамічний wear leveling |
| Повільне монтування | Швидке монтування |

## Перевірка після міграції

```cpp
void test_littlefs() {
    // Тест запису
    FILE* f = fopen("/storage/test.txt", "w");
    if (f) {
        fprintf(f, "LittleFS works!\n");
        fclose(f);
        ESP_LOGI(TAG, "Write test passed");
    }
    
    // Тест читання
    char buf[32];
    f = fopen("/storage/test.txt", "r");
    if (f) {
        fgets(buf, sizeof(buf), f);
        fclose(f);
        ESP_LOGI(TAG, "Read test passed: %s", buf);
    }
    
    // Інформація про FS
    size_t total, used;
    esp_littlefs_info("storage", &total, &used);
    ESP_LOGI(TAG, "LittleFS: %d/%d bytes used", used, total);
}
```

## Поради

1. **Backup даних** - Зробіть резервну копію важливих даних перед міграцією
2. **Тестування** - Ретельно протестуйте після міграції
3. **Моніторинг** - Слідкуйте за використанням пам'яті перші дні
4. **Документація** - Оновіть документацію проекту

## Результат

Після успішної міграції ви отримаєте:
- ✅ Кращу продуктивність для малих файлів
- ✅ Надійніший wear leveling
- ✅ Повну підтримку директорій
- ✅ Швидше монтування FS
- ✅ Кращу стійкість до збоїв живлення