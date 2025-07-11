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
        # Application subsystem
        "src/application/application.cpp"
        "src/application/application_wrapper.cpp"
        
        # Module Manager subsystem
        "src/modules/module_manager.cpp"
        "src/modules/module_lifecycle.cpp"
        "src/modules/module_heartbeat.cpp"
        
        # Config Manager subsystem
        "src/config/config_manager.cpp"
        "src/config/config_manager_async.cpp"
        
        # Event Bus subsystem
        "src/events/event_bus.cpp"
        
        # Shared State subsystem
        "src/state/shared_state.cpp"
        
        # Utilities
        "utils/diagnostic_tools.cpp"
        "utils/manifest_reader.cpp"
       
    INCLUDE_DIRS 
        "."
        "include"
        "src/application"
        "src/modules"
        "src/config"
        "src/events"
        "src/state"
        "utils"
        "${CMAKE_SOURCE_DIR}/main/generated"
    EMBED_FILES ${CONFIG_FILES}
    REQUIRES 
        esp_timer
        nvs_flash
        mittelab__nlohmann-json
        freertos
        base_module
        wifi_manager
        ESPhal
        joltwallet__littlefs
        vfs
        logger
        adaptive_ui
        sensor_drivers
    PRIV_REQUIRES
        esp_system
        log
)