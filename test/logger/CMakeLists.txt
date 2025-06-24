# Test CMakeLists.txt для LoggerModule

idf_component_register(
    SRCS 
        "test_logger_module.cpp"
    INCLUDE_DIRS 
        "."
        "${CMAKE_CURRENT_SOURCE_DIR}/../../components"
    REQUIRES 
        unity
        logger
        core
        base_module
    PRIV_REQUIRES
        esp_littlefs
)

# Додаємо Google Test якщо доступний
if(CONFIG_UNITY_ENABLE_DOUBLE)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        UNITY_INCLUDE_DOUBLE
    )
endif()
