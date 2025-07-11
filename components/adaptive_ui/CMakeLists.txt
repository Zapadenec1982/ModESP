# CMakeLists.txt for Adaptive UI component

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
        "adapters/lcd_ui/include"
        "adapters/mqtt_ui/include"
    REQUIRES 
        base_module
        mittelab__nlohmann-json
        esp_timer
        esp_http_server
        nvs_flash
        esp_wifi
    PRIV_REQUIRES
        log
        mbedtls
)
