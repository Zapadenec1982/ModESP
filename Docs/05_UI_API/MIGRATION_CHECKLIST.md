# ✅ Чек-лист міграції на adaptive_ui

## До міграції
- [ ] Переконатися, що проект компілюється
- [ ] Зробити git commit поточного стану

## Міграція (автоматично)
- [ ] Запустити: `python tools/migrate_to_adaptive_ui.py`
- [ ] Перевірити, що створено backup в `_archive_ui/`

## Після міграції (вручну)
- [ ] Оновити `components/core/CMakeLists.txt`:
  - Змінити `ui` на `adaptive_ui` в REQUIRES
  
- [ ] Виправити `components/core/src/application/application.cpp`:
  - Видалити `#include "api_dispatcher.h"`
  - Закоментувати блок з ConfigurationManager (рядки ~107-113)

- [ ] Перевірити збірку: `idf.py build`

- [ ] Протестувати:
  - [ ] Веб-інтерфейс працює на порту 80
  - [ ] API endpoints відповідають
  - [ ] LCD меню (якщо використовується)
  - [ ] MQTT (якщо використовується)

## Завершення
- [ ] Видалити `components/ui/` після успішного тестування
- [ ] Оновити документацію про зміни
- [ ] Git commit фінальної версії

## Якщо щось пішло не так
1. Відновити з backup: `cp -r components/_archive_ui/ui_backup_[timestamp] components/ui`
2. Повернути зміни в CMakeLists.txt та application.cpp
3. Звернутися за допомогою з описом помилки
