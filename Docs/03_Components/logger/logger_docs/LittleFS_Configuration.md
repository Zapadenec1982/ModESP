# LittleFS Configuration Guide for ModuChill

## Огляд

LittleFS (Little File System) - це файлова система, розроблена для вбудованих систем з обмеженими ресурсами. Вона оптимізована для надійності та ефективності використання flash пам'яті.

## Налаштування для ESP32

### 1. Таблиця розділів (partitions.csv)
```csv
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
phy_init, data, phy,     0xF000,   0x1000,
factory,  app,  factory, 0x10000,  0x200000,  # 2MB для програми
storage,  data, 0x82,    0x210000, 0x100000,  # 1MB для LittleFS
```

**Важливо**: SubType `0x82` вказує на LittleFS партицію.

### 2. Конфігурація в sdkconfig.defaults
```bash
# LittleFS Configuration
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"

# LittleFS параметри
CONFIG_LITTLEFS_MAX_PARTITIONS=1          # Кількість партицій
CONFIG_LITTLEFS_PAGE_SIZE=256             # Розмір сторінки
CONFIG_LITTLEFS_OBJ_NAME_LEN=64           # Максимальна довжина імені файлу
CONFIG_LITTLEFS_READ_SIZE=128             # Розмір буфера читання
CONFIG_LITTLEFS_PROG_SIZE=128             # Розмір буфера запису
CONFIG_LITTLEFS_BLOCK_SIZE=4096           # Розмір блоку
CONFIG_LITTLEFS_CACHE_SIZE=512            # Розмір кешу
CONFIG_LITTLEFS_LOOKAHEAD_SIZE=128        # Розмір lookahead буфера
CONFIG_LITTLEFS_BLOCK_CYCLES=512          # Цикли до wear leveling
CONFIG_LITTLEFS_USE_MTIME=y               # Підтримка часу модифікації
```

### 3. CMakeLists.txt залежності
```cmake
idf_component_register(
    SRCS 
        "your_source_files.cpp"
    REQUIRES 
        esp_littlefs
        # інші залежності
)
```

## Використання в коді

### Ініціалізація
```cpp
#include <esp_littlefs.h>

esp_err_t init_littlefs() {
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/storage",           // Точка монтування
        .partition_label = "storage",      // Назва партиції
        .format_if_mount_failed = true,    // Форматувати якщо не вдалося
        .dont_mount = false                // Монтувати одразу
    };
    
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", 
                     esp_err_to_name(ret));
        }
        return ret;
    }
    
    // Перевірка інформації про партицію
    size_t total = 0, used = 0;
    ret = esp_littlefs_info("storage", &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    
    return ESP_OK;
}
```

### Робота з файлами
```cpp
// Запис у файл
FILE* f = fopen("/storage/data.txt", "w");
if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
}
fprintf(f, "Hello LittleFS!\n");
fclose(f);

// Читання з файлу
char line[64];
f = fopen("/storage/data.txt", "r");
if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
}
fgets(line, sizeof(line), f);
fclose(f);

// Перевірка існування файлу
struct stat st;
if (stat("/storage/data.txt", &st) == 0) {
    ESP_LOGI(TAG, "File exists, size: %ld", st.st_size);
}
```

### Операції з директоріями
```cpp
// Створення директорії
mkdir("/storage/logs", 0755);

// Читання вмісту директорії
DIR* dir = opendir("/storage");
struct dirent* entry;
while ((entry = readdir(dir)) != NULL) {
    ESP_LOGI(TAG, "Found file: %s", entry->d_name);
}
closedir(dir);

// Видалення файлу
unlink("/storage/old_file.txt");

// Перейменування
rename("/storage/old_name.txt", "/storage/new_name.txt");
```

## Оптимізація продуктивності

### 1. Буферизація записів
```cpp
// Використовуйте setvbuf для покращення продуктивності
FILE* f = fopen("/storage/log.txt", "a");
char buffer[512];
setvbuf(f, buffer, _IOFBF, sizeof(buffer));
// Тепер записи буферизуються
```

### 2. Пакетні операції
```cpp
// Замість багатьох малих записів
for(int i = 0; i < 100; i++) {
    FILE* f = fopen("/storage/data.txt", "a");
    fprintf(f, "Line %d\n", i);
    fclose(f);  // Погано!
}

// Краще відкрити файл один раз
FILE* f = fopen("/storage/data.txt", "a");
for(int i = 0; i < 100; i++) {
    fprintf(f, "Line %d\n", i);
}
fclose(f);  // Добре!
```

### 3. Асинхронний запис
```cpp
// Використовуйте окрему задачу для запису
xQueueHandle writeQueue;

void writer_task(void* param) {
    char data[256];
    while(1) {
        if(xQueueReceive(writeQueue, data, portMAX_DELAY)) {
            FILE* f = fopen("/storage/async_log.txt", "a");
            fputs(data, f);
            fclose(f);
        }
    }
}
```

## Обслуговування файлової системи

### Форматування
```cpp
esp_littlefs_format("storage");
```

### Перевірка цілісності
```cpp
// LittleFS автоматично перевіряє та виправляє помилки
// при монтуванні
```

### Моніторинг використання
```cpp
void check_fs_usage() {
    size_t total = 0, used = 0;
    esp_littlefs_info("storage", &total, &used);
    
    float percent = ((float)used / total) * 100;
    ESP_LOGI(TAG, "FS Usage: %.1f%% (%d/%d bytes)", 
             percent, used, total);
    
    if (percent > 90) {
        ESP_LOGW(TAG, "File system almost full!");
    }
}
```

## Порівняння з SPIFFS

| Характеристика | LittleFS | SPIFFS |
|---------------|----------|---------|
| Wear leveling | ✓ Динамічний | ✗ Статичний |
| Директорії | ✓ Повна підтримка | ✗ Емуляція |
| Швидкість | Швидша для малих файлів | Повільніша |
| RAM використання | ~4KB | ~6KB |
| Стійкість до збоїв | Висока | Середня |
| Час монтування | Швидкий | Повільний для великих FS |

## Типові проблеми та рішення

### 1. Помилка монтування
```cpp
// Спробуйте форматування
esp_vfs_littlefs_conf_t conf = {
    .base_path = "/storage",
    .partition_label = "storage",
    .format_if_mount_failed = true,  // Автоформатування
    .dont_mount = false
};
```

### 2. Повільний запис
- Використовуйте буферизацію
- Групуйте операції запису
- Розгляньте асинхронний запис

### 3. Фрагментація
- LittleFS автоматично дефрагментує
- Періодично перезаписуйте великі файли

## Рекомендації для промислового використання

1. **Резервування критичних даних** - дублюйте в NVS
2. **Ротація логів** - видаляйте старі файли автоматично
3. **Моніторинг** - слідкуйте за використанням простору
4. **Wear leveling** - використовуйте CONFIG_LITTLEFS_BLOCK_CYCLES
5. **Перевірка після збою** - LittleFS відновлюється автоматично

## Корисні макроси

```cpp
#define FS_CHECK(x) do {                                         \
    esp_err_t err = (x);                                        \
    if (err != ESP_OK) {                                        \
        ESP_LOGE(TAG, "FS Error: %s", esp_err_to_name(err));   \
        return err;                                             \
    }                                                           \
} while(0)

#define FS_PATH "/storage"
#define MAX_FILE_SIZE (64 * 1024)  // 64KB
```