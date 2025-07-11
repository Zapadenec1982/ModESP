// test_adaptive_ui.cpp
// Simple test for Phase 5 Adaptive UI

#include <stdio.h>
#include "esp_log.h"
// TODO: Fix include paths
// #include "ui_filter.h"
// #include "lazy_component_loader.h"
// #include "generated_ui_components.h"
#include "nlohmann/json.hpp"

static const char* TAG = "AdaptiveUITest";

extern "C" void test_adaptive_ui() {
    ESP_LOGI(TAG, "=== Phase 5 Adaptive UI Test ===");
    
    // TODO: Fix include paths before enabling this test
    ESP_LOGW(TAG, "Test disabled due to include path issues");
    return;
    
    // 1. Test configuration
    nlohmann::json config = {
        {"sensor", {
            {"type", "DS18B20"},
            {"count", 2}
        }},
        {"system", {
            {"debug", true}
        }}
    };
    
    // 2. Initialize UI Filter
    ESP_LOGI(TAG, "Initializing UI Filter...");
    ModESP::UI::UIFilter filter;
    filter.init(config, ModESP::UI::UserRole::TECHNICIAN);
    
    // 3. Filter components
    ESP_LOGI(TAG, "Filtering %zu total components...", ModESP::UI::COMPONENT_COUNT);
    auto visible = filter.filterComponents(
        ModESP::UI::ALL_COMPONENTS,
        ModESP::UI::COMPONENT_COUNT
    );
    
    ESP_LOGI(TAG, "Visible components: %zu", visible.size());
    for (const auto* comp : visible) {
        ESP_LOGI(TAG, "  - %s (type: %d, access: %d)", 
                 comp->id, 
                 static_cast<int>(comp->type),
                 static_cast<int>(comp->min_access));
    }
    
    // 4. Test Lazy Loader
    ESP_LOGI(TAG, "Testing Lazy Loader...");
    ModESP::UI::LazyComponentLoader& loader = 
        ModESP::UI::LazyLoaderManager::getInstance();
    
    // Register factories (normally done at startup)
    ModESP::UI::registerAllComponentFactories(loader);
    
    // Load a component
    auto* component = loader.getComponent("sensor_type_selector");
    if (component) {
        ESP_LOGI(TAG, "Successfully loaded: %s", component->getId().c_str());
    }
    
    // 5. Check statistics
    auto stats = loader.getStats();
    ESP_LOGI(TAG, "Loader stats:");
    ESP_LOGI(TAG, "  - Components loaded: %zu", stats.components_loaded);
    ESP_LOGI(TAG, "  - Cache size: %zu bytes", stats.cache_size_bytes);
    ESP_LOGI(TAG, "  - Hit rate: %.2f%%", stats.hit_rate * 100);
    
    // 6. Test condition change
    ESP_LOGI(TAG, "Changing sensor type to NTC...");
    config["sensor"]["type"] = "NTC";
    filter.init(config, ModESP::UI::UserRole::TECHNICIAN);
    
    auto new_visible = filter.filterComponents(
        ModESP::UI::ALL_COMPONENTS,
        ModESP::UI::COMPONENT_COUNT
    );
    
    ESP_LOGI(TAG, "Visible components after change: %zu", new_visible.size());
    
    ESP_LOGI(TAG, "=== Test Complete ===");
}

// Add to app_main() or call separately
extern "C" void app_main() {
    test_adaptive_ui();
}
