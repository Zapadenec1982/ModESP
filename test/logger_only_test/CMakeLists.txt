# The following lines of boilerplate have to be in your project's
# CMakeLists.txt, but you can put them after project() and add other CMake
# commands too.

cmake_minimum_required(VERSION 3.16)

# Set component search paths
set(EXTRA_COMPONENT_DIRS 
    "../../components"
    "components"
)

# Use custom partition table
set(PARTITION_TABLE_CSV_PATH "${CMAKE_SOURCE_DIR}/partitions.csv")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(logger_test) 