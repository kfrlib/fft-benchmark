cmake_minimum_required(VERSION 3.12)

list(PREPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

project(fft_benchmark)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

add_compile_definitions(CMAKE_CXX_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}")
add_compile_definitions(CMAKE_CXX_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(COMMON_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/src)

set(COMMON_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/fft_benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/benchmark.hpp
                ${CMAKE_CURRENT_LIST_DIR}/src/benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/utils.cpp)

function (add_fft NAME)
    cmake_parse_arguments(LIB "" "" "SOURCES;LIBRARIES;INCLUDES" ${ARGN})

    add_executable(fft_benchmark_${NAME} ${COMMON_SRCS} ${LIB_SOURCES})
    target_link_libraries(fft_benchmark_${NAME} PUBLIC ${LIB_LIBRARIES})
    target_include_directories(fft_benchmark_${NAME} PUBLIC ${COMMON_INCLUDES} ${LIB_INCLUDES})
    
endfunction ()

add_subdirectory(fftlibs)
