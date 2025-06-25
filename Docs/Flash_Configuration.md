# Конфігурація Flash пам'яті для ModuChill

## Загальна інформація

ModuChill використовує 4MB SPI Flash пам'ять з оптимізованою таблицею розділів для підтримки:
- OTA (Over-The-Air) оновлень
- LittleFS файлової системи
- Достатнього простору для програми

## Таблиця розділів

```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,     # 20KB - Non-volatile storage
otadata,  data, ota,     0xE000,   0x2000,     # 8KB - OTA data
phy_init, data, phy,     0x10000,  0x1000,     # 4KB - PHY init data
factory,  app,  factory, 0x11000,  0x170000,   # 1.44MB - Factory app
ota_0,    app,  ota_0,   0x181000, 0x170000,   # 1.44MB - OTA app slot
storage,  data, littlefs,0x2F1000, 0x100000,   # 1MB - File system
```

## Розподіл пам'яті

### Загальний розмір: 4MB (0x400000)

1. **Bootloader**: 0x0000 - 0x8FFF (36KB)
   - ESP32 bootloader
   - Не вказується в таблиці розділів

2. **Partition Table**: 0x9000 - 0x8FFF (4KB)
   - Сама таблиця розділів
   - Автоматично генерується

3. **NVS**: 0x9000 - 0xDFFF (20KB)
   - Зберігання налаштувань
   - WiFi credentials
   - Системні параметри

4. **OTA Data**: 0xE000 - 0xFFFF (8KB)
   - Інформація про активний розділ
   - Лічильник оновлень

5. **PHY Init**: 0x10000 - 0x10FFF (4KB)
   - Калібрувальні дані RF

6. **Factory App**: 0x11000 - 0x180FFF (1.44MB)
   - Основна програма
   - Завжди доступна як fallback

7. **OTA_0**: 0x181000 - 0x2F0FFF (1.44MB)
   - Слот для OTA оновлень
   - Альтернативна програма

8. **LittleFS**: 0x2F1000 - 0x3F0FFF (1MB)
   - Файлова система
   - Логи, конфігурації, дані

9. **Резерв**: 0x3F1000 - 0x3FFFFF (60KB)
   - Вільний простір для майбутніх потреб

## Налаштування в sdkconfig

```bash
# Flash size
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"

# Flash frequency
CONFIG_ESPTOOLPY_FLASHFREQ_80M=y

# Partition table
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
```

## OTA оновлення

### Процес оновлення:
1. Завантаження нової прошивки в OTA_0
2. Перевірка цілісності
3. Встановлення OTA_0 як активного розділу
4. Перезавантаження
5. У випадку помилки - повернення до factory

### API для OTA:
```cpp
#include "esp_ota_ops.h"

// Початок OTA
esp_ota_handle_t update_handle = 0;
const esp_partition_t* update_partition = esp_ota_get_next_update_partition(NULL);
esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);

// Запис даних
esp_ota_write(update_handle, data, data_len);

// Завершення
esp_ota_end(update_handle);
esp_ota_set_boot_partition(update_partition);
esp_restart();
```

## Оптимізація використання

### Зменшення розміру програми:
1. **Оптимізація компіляції**:
   ```bash
   CONFIG_COMPILER_OPTIMIZATION_SIZE=y
   CONFIG_BOOTLOADER_COMPILER_OPTIMIZATION_SIZE=y
   ```

2. **Видалення непотрібних компонентів**:
   ```bash
   CONFIG_BT_ENABLED=n        # Якщо не використовується Bluetooth
   CONFIG_ESP32_ENABLE_COREDUMP=n
   ```

3. **Налаштування логів**:
   ```bash
   CONFIG_LOG_DEFAULT_LEVEL_WARN=y
   CONFIG_BOOTLOADER_LOG_LEVEL_WARN=y
   ```

### Моніторинг використання:
```cpp
// Перевірка розміру програми
const esp_partition_t* running = esp_ota_get_running_partition();
ESP_LOGI(TAG, "Running partition: %s, size: %d", 
         running->label, running->size);

// Статистика LittleFS
size_t total = 0, used = 0;
esp_littlefs_info("storage", &total, &used);
ESP_LOGI(TAG, "Storage: %d/%d bytes (%.1f%%)", 
         used, total, (float)used/total*100);
```

## Рекомендації

1. **Завжди залишайте factory розділ** - це гарантія відновлення
2. **Тестуйте OTA оновлення** - перед розгортанням
3. **Моніторте простір** - особливо в LittleFS
4. **Резервуйте дані** - критичні налаштування в NVS
5. **Плануйте розширення** - залишайте резерв

## Типові помилки

### "Partition table too large"
- Перевірте розмір flash: `esptool.py flash_id`
- Переконайтеся що CONFIG_ESPTOOLPY_FLASHSIZE="4MB"

### "OTA image invalid"
- Перевірте що розмір OTA <= розміру розділу
- Використовуйте esp_ota_get_next_update_partition()

### "LittleFS mount failed"
- Форматуйте при першому запуску
- Перевірте цілісність розділу