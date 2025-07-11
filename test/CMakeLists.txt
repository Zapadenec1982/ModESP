cmake_minimum_required(VERSION 3.16)

# Set project name
set(PROJECT_NAME "modesp_tests")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define the project
project(${PROJECT_NAME})

# Add test component path
set(EXTRA_COMPONENT_DIRS "../components" "components")

# Enable testing
set(CONFIG_UNITY_ENABLE_FIXTURE 1)
set(CONFIG_UNITY_ENABLE_BACKTRACE_ON_FAIL 1) 