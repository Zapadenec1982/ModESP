cmake_minimum_required(VERSION 3.16)

# Set component search paths
set(EXTRA_COMPONENT_DIRS 
    "../../components"
    "components"
)

# Use custom partition table for testing
set(PARTITION_TABLE_CSV_PATH "${CMAKE_SOURCE_DIR}/partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(integration_test) 