idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "." "generated"
    REQUIRES 
        core
        base_module
        unity
        adaptive_ui
)