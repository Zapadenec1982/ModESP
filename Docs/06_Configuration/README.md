# ⚙️ Configuration - Конфігурація та налаштування

Документація по конфігурації плат, flash пам'яті та системних налаштувань.

## 📚 Документи
- **[board_update_procedure.md](board_update_procedure.md)** - Процедура оновлення конфігурації плат
- **[board_changes_summary.md](board_changes_summary.md)** - Підсумок змін у платах
- **[custom_board_config.md](custom_board_config.md)** - Конфігурація користувацьких плат
- **[flash_configuration.md](flash_configuration.md)** - Конфігурація 4MB Flash з OTA
- **[no_exceptions_config.md](no_exceptions_config.md)** - Конфігурація без C++ exceptions

## 🎯 Підтримувані плати
- **ESP32-S3** - рекомендована платформа (оптимізована)
- **ESP32** - стандартна підтримка
- **ESP32-C3, ESP32-S2** - обмежена підтримка

## 🚀 Додавання нової плати
1. Створіть `board_config.h`
2. Налаштуйте GPIO mapping
3. Оновіть Kconfig
4. Тестування на реальному заліві

---
*Документація оновлена: 2025-06-29*
