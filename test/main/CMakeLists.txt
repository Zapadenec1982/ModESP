idf_component_register(
    SRCS "test_module_heartbeat.cpp" "test_memory_pool.cpp" "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        unity
        test_runner
) 