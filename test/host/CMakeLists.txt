cmake_minimum_required(VERSION 3.16)
project(modesp_tests)

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/base_module
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/core
    ${CMAKE_CURRENT_SOURCE_DIR}/../../components/core/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../managed_components/mittelab__nlohmann-json
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks  # For mock ESP-IDF headers
)

# Mock ESP-IDF headers
add_library(esp_mocks INTERFACE)

# Test executable
add_executable(run_tests
    test_module_manager.cpp
    test_event_bus.cpp
    test_shared_state.cpp
    # Add mock implementations
    mocks/esp_mock_impl.cpp
)

# Link libraries
target_link_libraries(run_tests
    gtest_main
    esp_mocks
)

# Add test discovery
include(GoogleTest)
gtest_discover_tests(run_tests)
