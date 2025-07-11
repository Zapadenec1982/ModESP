# ConfigManager Lifecycle: Checklist імплементації

## ✅ Що потрібно зробити:

### 1. Оновити config_manager.h
- [ ] Додати нові API функції:
  - `enable_async_save()`
  - `enable_auto_save(bool)`
  - `force_save_sync()`
  - `get_save_status()`
  - `save_module_async(string)`

### 2. Оновити config_manager.cpp
- [ ] Розділити `save()` на sync/async версії
- [ ] Додати флаги `async_save_available` та `auto_save_enabled`
- [ ] Реалізувати `enable_async_save()` - ініціалізує async після старту
- [ ] Оновити `set()` для auto-save
- [ ] Додати watchdog feed в `load()` та `save_sync()`

### 3. Інтегрувати config_manager_async
- [ ] Включити async файли в CMakeLists.txt
- [ ] Додати Kconfig опції
- [ ] Ініціалізувати async ПІСЛЯ старту модулів

### 4. Оновити Application
- [ ] Правильний порядок в `init()`:
  ```cpp
  ConfigManager::init();
  ConfigManager::load();  // Блокуюче!
  ModuleManager::configure_all();
  ModuleManager::init_all();
  ConfigManager::enable_async_save();  // Після модулів!
  ```
- [ ] Використати `force_save_sync()` в `stop()`

### 5. Оновити тести
- [ ] Додати test_config_lifecycle.cpp
- [ ] Перевірити синхронність load()
- [ ] Перевірити async save під час runtime
- [ ] Перевірити sync save при shutdown

### 6. Документація
- [ ] Оновити README з lifecycle
- [ ] Додати приклади використання
- [ ] Описати коли що використовувати

## 🔍 Що перевірити:

1. **Load блокує при старті?** ✓
2. **Модулі отримують config?** ✓
3. **Async включається тільки після старту?** ✓
4. **Runtime saves не блокують?** ✓
5. **Shutdown saves гарантовані?** ✓
6. **Watchdog не спрацьовує?** ✓

## ⚠️ Критичні моменти:

- `load()` НІКОЛИ не може бути async!
- Async save включати ТІЛЬКИ після init модулів
- Critical config changes = force sync save
- Shutdown ЗАВЖДИ sync

## 📈 Очікувані результати:

- Детермінований старт системи
- Відсутність watchdog timeout
- Responsive UI під час saves
- Надійне збереження при shutdown
- Зрозуміла і проста архітектура
