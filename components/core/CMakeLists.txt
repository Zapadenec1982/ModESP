# Modular configuration files to embed
set(CONFIG_FILES
    "configs/system.json"
    "configs/climate.json"
    "configs/sensors.json"
    "configs/actuators.json"
    "configs/alarms.json"
    "configs/network.json"
    "configs/ui.json"
    "configs/logging.json"
    "configs/wifi.json"
    "configs/rtc.json"
)

# Register component with embedded configs
idf_component_register(
    SRCS 
        "application.cpp"
        "application_wrapper.cpp"
        "config_manager.cpp"
        "event_bus.cpp"
        "module_manager.cpp"
        "module_registry.cpp"
        "shared_state.cpp"
    INCLUDE_DIRS "."
    EMBED_FILES ${CONFIG_FILES}
    REQUIRES 
        esp_timer
        nvs_flash
        mittelab__nlohmann-json
        freertos
        base_module
        wifi_manager
        ESPhal
    PRIV_REQUIRES
        esp_system
        log
)