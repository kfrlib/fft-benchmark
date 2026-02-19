cmake_minimum_required(VERSION 3.12)

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
                ${CMAKE_CURRENT_LIST_DIR}/src/benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/utils.cpp
                ${CMAKE_CURRENT_LIST_DIR}/src/vectors.c)

enable_testing()

set(SIZES 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576)

function (add_fft NAME)
    cmake_parse_arguments(LIB "" "" "SOURCES;LIBRARIES;INCLUDES;DEFINITIONS" ${ARGN})

    add_executable(fft_benchmark_${NAME} ${COMMON_SRCS} ${LIB_SOURCES})
    target_link_libraries(fft_benchmark_${NAME} PRIVATE ${LIB_LIBRARIES})
    target_include_directories(fft_benchmark_${NAME} PRIVATE ${COMMON_INCLUDES} ${LIB_INCLUDES})
    target_compile_definitions(fft_benchmark_${NAME} PRIVATE ${LIB_DEFINITIONS})

    add_test(NAME benchmark_${NAME} COMMAND fft_benchmark_${NAME} --save ${CMAKE_BINARY_DIR}/results_${NAME}.json
                                            --no-banner --no-progress ${SIZES} --complex y --inplace n --inverse n)
endfunction ()

add_subdirectory(fftlibs)
