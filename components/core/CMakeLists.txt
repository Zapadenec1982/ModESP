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
)

# Register component with embedded configs
idf_component_register(
    SRCS 
        "application.cpp"
        "config_manager.cpp"
        "event_bus.cpp"
        "module_manager.cpp"
        "shared_state.cpp"
    INCLUDE_DIRS "."
    EMBED_FILES ${CONFIG_FILES}
    REQUIRES 
        esp_timer
        nvs_flash
        nlohmann_json
        freertos
        base_module
    PRIV_REQUIRES
        esp_system
        log
)