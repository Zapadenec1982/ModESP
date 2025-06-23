# UI Generation CMake configuration

# Find Python
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Collect all ui_schema.json files
file(GLOB_RECURSE UI_SCHEMA_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*/ui_schema.json"
)

# Output directory for generated files
set(UI_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/main/generated")

# Generated file list
set(UI_GENERATED_FILES
    "${UI_GENERATED_DIR}/web_ui_generated.h"
    "${UI_GENERATED_DIR}/mqtt_topics_generated.h" 
    "${UI_GENERATED_DIR}/lcd_menu_generated.h"
    "${UI_GENERATED_DIR}/ui_registry_generated.h"
)

# Create output directory
file(MAKE_DIRECTORY ${UI_GENERATED_DIR})

# Custom command to run UI generator
add_custom_command(
    OUTPUT ${UI_GENERATED_FILES}
    COMMAND ${Python3_EXECUTABLE} 
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/ui_generator.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/components"
            "${UI_GENERATED_DIR}"
    DEPENDS ${UI_SCHEMA_FILES}
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/ui_generator.py"
    COMMENT "Generating UI files from schemas..."
    VERBATIM
)

# Custom target for UI generation
add_custom_target(generate_ui ALL
    DEPENDS ${UI_GENERATED_FILES}
)

# Add to component requirements
idf_component_get_property(main_lib main COMPONENT_LIB)
add_dependencies(${main_lib} generate_ui)

# Include generated directory in build
target_include_directories(${main_lib} PRIVATE ${UI_GENERATED_DIR})