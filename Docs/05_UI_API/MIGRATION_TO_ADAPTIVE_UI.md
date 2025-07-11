# План міграції з ui/ на adaptive_ui/

## 🎯 Мета
Повністю перейти на adaptive_ui/ та видалити застарілу ui/ систему.

## 📊 Аналіз залежностей

### Що використовується з ui/:
1. **api_dispatcher** - включається в application.cpp (але можливо не використовується)
2. **web_ui_module** - HTTP сервер та веб-інтерфейс
3. **CMake залежність** - core REQUIRES ui

### Що вже є в adaptive_ui/:
- ✅ ui_filter - фільтрація компонентів
- ✅ lazy_component_loader - оптимізація пам'яті
- ✅ ui_component_base - базові класи
- ✅ Адаптери для LCD та MQTT
- ❌ Веб-адаптер
- ❌ API dispatcher

## 🚀 План міграції

### Phase 1: Перенести веб-функціонал (1-2 дні)

1. **Створити веб-адаптер в adaptive_ui**:
```
adaptive_ui/
├── adapters/
│   ├── web/
│   │   ├── include/
│   │   │   ├── web_ui_adapter.h
│   │   │   └── api_handler.h
│   │   └── src/
│   │       ├── web_ui_adapter.cpp
│   │       └── api_handler.cpp
```

2. **Портувати функціонал**:
   - HTTP сервер з web_ui_module → web_ui_adapter
   - API обробку з api_dispatcher → api_handler
   - Адаптувати під архітектуру adaptive_ui

### Phase 2: Оновити залежності (1 день)

1. **Оновити CMakeLists.txt в core**:
```cmake
REQUIRES 
    # ... інші залежності
    adaptive_ui  # замість ui
```

2. **Оновити include в application.cpp**:
```cpp
// Видалити або замінити
// #include "api_dispatcher.h"
```

3. **Оновити CMakeLists.txt в adaptive_ui**:
```cmake
idf_component_register(
    SRCS 
        "ui_filter.cpp"
        "lazy_component_loader.cpp"
        "adapters/web/src/web_ui_adapter.cpp"
        "adapters/web/src/api_handler.cpp"
    INCLUDE_DIRS 
        "." 
        "include"
        "adapters/web/include"
    REQUIRES 
        base_module
        esp_http_server  # для веб-сервера
        mittelab__nlohmann-json
        esp_timer
        esp_log
)
```

### Phase 3: Тестування (1 день)

1. Переконатися, що веб-інтерфейс працює
2. Перевірити API endpoints
3. Протестувати LCD та MQTT адаптери

### Phase 4: Видалення ui/ (після тестування)

1. **Архівувати**:
```bash
mv components/ui components/_archive_ui/old_ui_system
```

2. **Оновити документацію**

## 📝 Код для перенесення

### web_ui_adapter.h (базовий приклад):
```cpp
#pragma once

#include "ui_component_base.h"
#include "esp_http_server.h"
#include <memory>

namespace ModESP::UI {

class WebUIAdapter {
private:
    httpd_handle_t server_ = nullptr;
    UIFilter* filter_;
    LazyComponentLoader* loader_;
    
public:
    WebUIAdapter(UIFilter* filter, LazyComponentLoader* loader);
    ~WebUIAdapter();
    
    esp_err_t start(uint16_t port = 80);
    void stop();
    
    // HTTP handlers
    esp_err_t handleGetUI(httpd_req_t* req);
    esp_err_t handleApiRequest(httpd_req_t* req);
    
    // Render components to HTML/JSON
    std::string renderComponents();
    nlohmann::json getComponentsJson();
};

} // namespace ModESP::UI
```

### api_handler.h:
```cpp
#pragma once

#include "nlohmann/json.hpp"
#include <functional>
#include <map>

namespace ModESP::UI {

class ApiHandler {
public:
    using Handler = std::function<nlohmann::json(const nlohmann::json&)>;
    
    void registerEndpoint(const std::string& path, Handler handler);
    nlohmann::json handleRequest(const std::string& path, const nlohmann::json& params);
    
private:
    std::map<std::string, Handler> endpoints_;
};

} // namespace ModESP::UI
```

## ⚡ Швидкі дії для початку

1. **Створити структуру для веб-адаптера**:
```bash
cd components/adaptive_ui/adapters
mkdir -p web/{include,src}
```

2. **Скопіювати базовий код**:
```bash
# Використати web_ui_module як основу
cp ../ui/include/web_ui_module.h web/include/web_ui_adapter.h
cp ../ui/src/web_ui_module.cpp web/src/web_ui_adapter.cpp
```

3. **Адаптувати під нову архітектуру**:
   - Інтегрувати з UIFilter
   - Використати LazyComponentLoader
   - Підтримати динамічне рендерування

## 🎯 Переваги після міграції

- ✅ Єдина UI система
- ✅ Менше коду для підтримки
- ✅ Краща архітектура (adaptive)
- ✅ Економія RAM через lazy loading
- ✅ Легше додавати нові адаптери

## ⚠️ Ризики

- Веб-інтерфейс може тимчасово не працювати
- Потрібно оновити всі залежності
- Можливі проблеми з API сумісністю
