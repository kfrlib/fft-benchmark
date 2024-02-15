# CMake Configuration
cmake_minimum_required(VERSION 3.12)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

add_compile_definitions(CMAKE_CXX_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}")
add_compile_definitions(CMAKE_CXX_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}")

# Project information & includes
project(fft_benchmark)
include(DisableInSourceBuild)
include(FindPackageStandard)

# Adding libraries
add_subdirectory(src)
