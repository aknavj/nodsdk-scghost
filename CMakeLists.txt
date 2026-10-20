cmake_minimum_required(VERSION 3.16)
project(nodsdk-scghost C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build output goes to <build>/cmake/ so the old Makefile layout still works
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cmake)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cmake)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/cmake)

option(NODSDK_BUILD_VIEWER "Build the OpenGL asset viewer" ON)
option(NODSDK_BUILD_PARSERS "Build the C++ parser library explicitly (also required by NODscene)" ON)

include(CTest)
if(NODSDK_BUILD_PARSERS OR NODSDK_BUILD_VIEWER)
    add_subdirectory(src/parsers)
endif()
add_subdirectory(src/tools)
if(NODSDK_BUILD_VIEWER)
    add_subdirectory(src/viewer)
endif()
