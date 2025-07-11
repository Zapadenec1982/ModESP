// Приклад використання WebUIAdapter з adaptive_ui
#include "web_ui_adapter.h"
#include "ui_filter.h"
#include "lazy_component_loader.h"

using namespace ModESP::UI;

void setup_web_ui() {
    // Ініціалізація фільтра
    UIFilter filter;
    filter.setConfig(system_config);
    filter.setUserRole(UserRole::TECHNICIAN);
    
    // Ініціалізація loader
    LazyComponentLoader& loader = LazyLoaderManager::getInstance();
    
    // Створити та запустити веб-адаптер
    auto web_adapter = std::make_unique<WebUIAdapter>(&filter, &loader);
    
    if (web_adapter->start(80) == ESP_OK) {
        ESP_LOGI("WebUI", "Web interface started on port 80");
    }
}
