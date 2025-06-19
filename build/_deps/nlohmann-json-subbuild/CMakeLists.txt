# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.30.2)

# Reject any attempt to use a toolchain file. We must not use one because
# we could be downloading it here. If the CMAKE_TOOLCHAIN_FILE environment
# variable is set, the cache variable will have been initialized from it.
unset(CMAKE_TOOLCHAIN_FILE CACHE)
unset(ENV{CMAKE_TOOLCHAIN_FILE})

# We name the project and the target for the ExternalProject_Add() call
# to something that will highlight to the user what we are working on if
# something goes wrong and an error message is produced.

project(nlohmann-json-populate NONE)



include(ExternalProject)
ExternalProject_Add(nlohmann-json-populate
                     "UPDATE_DISCONNECTED" "False" "EXTERNALPROJECT_INTERNAL_ARGUMENT_SEPARATOR" "URL" "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp" "URL_HASH" "SHA256=9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6" "DOWNLOAD_NAME" "json_impl.hpp" "DOWNLOAD_DIR" "C:/ModESP/managed_components/mittelab__nlohmann-json/nlohmann" DOWNLOAD_NO_EXTRACT YES
                    SOURCE_DIR          "C:/ModESP/build/_deps/nlohmann-json-src"
                    BINARY_DIR          "C:/ModESP/build/_deps/nlohmann-json-build"
                    CONFIGURE_COMMAND   ""
                    BUILD_COMMAND       ""
                    INSTALL_COMMAND     ""
                    TEST_COMMAND        ""
                    USES_TERMINAL_DOWNLOAD  YES
                    USES_TERMINAL_UPDATE    YES
                    USES_TERMINAL_PATCH     YES
)


ExternalProject_Get_Property(nlohmann-json-populate DOWNLOADED_FILE)
get_filename_component(dlFileName "${DOWNLOADED_FILE}" NAME)

ExternalProject_Add_Step(nlohmann-json-populate copyfile
  COMMAND    "C:/Users/User/esp/v5.3.3/esp-idf/tools/tools/cmake/3.30.2/bin/cmake.exe" -E copy_if_different
             "<DOWNLOADED_FILE>" "C:/ModESP/build/_deps/nlohmann-json-src"
  DEPENDEES  patch
  DEPENDERS  configure
  BYPRODUCTS "C:/ModESP/build/_deps/nlohmann-json-src/${dlFileName}"
  COMMENT    "Copying file to SOURCE_DIR"
)

