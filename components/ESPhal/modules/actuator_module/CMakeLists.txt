idf_component_register(
    SRCS 
        "src/actuator_module.cpp"
    INCLUDE_DIRS 
        "include"
    REQUIRES
        base_module
        ESPhal
        core
        nlohmann_json
        actuator_drivers      # Base actuator driver interfaces
    PRIV_REQUIRES
        esp_timer
)

# Include specific actuator drivers based on configuration
# The drivers will be automatically linked if enabled in menuconfig
if(CONFIG_ENABLE_RELAY_DRIVER)
    list(APPEND extra_component_dirs "${CMAKE_CURRENT_LIST_DIR}/../../actuator_drivers/relay")
endif()

if(CONFIG_ENABLE_PWM_DRIVER)
    list(APPEND extra_component_dirs "${CMAKE_CURRENT_LIST_DIR}/../../actuator_drivers/pwm")
endif()

if(CONFIG_ENABLE_STEPPER_DRIVER)
    list(APPEND extra_component_dirs "${CMAKE_CURRENT_LIST_DIR}/../../actuator_drivers/stepper")
endif()