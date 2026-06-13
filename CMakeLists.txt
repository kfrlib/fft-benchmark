cmake_minimum_required(VERSION 3.12)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

if (APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/libomp)
endif ()

set(VCPKG_ROOT ${CMAKE_SOURCE_DIR}/vcpkg)
set(VCPKG_MANIFEST_MODE
    TRUE
    CACHE BOOL "")
set(ENV{VCPKG_ROOT} ${VCPKG_ROOT})

set(VCPKG_INSTALL_OPTIONS "--allow-unsupported")

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64")
    set(VCPKG_TARGET_TRIPLET "arm64-osx")
    set(VCPKG_HOST_TRIPLET "arm64-osx")
elseif (WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static-md")
    set(VCPKG_HOST_TRIPLET "x64-windows-static-md")
else ()
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE HOST_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (HOST_ARCH MATCHES "aarch64|arm64")
        set(VCPKG_TARGET_TRIPLET "arm64-linux")
        set(VCPKG_HOST_TRIPLET "arm64-linux")
    else ()
        set(VCPKG_TARGET_TRIPLET "x64-linux")
        set(VCPKG_HOST_TRIPLET "x64-linux")
    endif ()
endif ()

set(CMAKE_TOOLCHAIN_FILE
    ${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
    CACHE PATH "")
set(VCPKG_INSTALLED_DIR
    ${CMAKE_SOURCE_DIR}/vcpkg_installed
    CACHE PATH "")

project(fft_benchmark)

if (MSVC)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif ()

add_compile_definitions(CMAKE_CXX_COMPILER_ID="${CMAKE_CXX_COMPILER_ID}")
add_compile_definitions(CMAKE_CXX_COMPILER_VERSION="${CMAKE_CXX_COMPILER_VERSION}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(COMMON_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/src)

set(COMMON_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/benchmark.hpp)
set(FFT_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/fft_benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/fft_benchmark.hpp
             ${CMAKE_CURRENT_LIST_DIR}/src/fft_vectors.c)
set(SRC_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/src_benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/src_benchmark.hpp)

enable_testing()

set(FFT_SIZES
    16
    32
    64
    128
    256
    512
    1024
    2048
    4096
    8192
    16384
    32768
    65536
    131072
    262144
    524288
    1048576)

option(OPEN "Only include open-source libraries" ON)

if (OPEN)
    message(STATUS "OPEN is ON: compiling open-source libraries only (closed-source libraries excluded)")
    message(STATUS "  Resulting package is GPLv3-compatible")
else ()
    message(STATUS "OPEN is OFF: compiling closed-source libraries only (open-source libraries excluded)")
endif ()
message(STATUS "  Note: each library has its own license")

function (add_benchmark NAME)
    cmake_parse_arguments(LIB "FFT;SRC" "LICENSE" "SOURCES;LIBRARIES;INCLUDES;DEFINITIONS" ${ARGN})

    if (NOT LIB_FFT AND NOT LIB_SRC)
        message(FATAL_ERROR "Either FFT or SRC must be specified for benchmark ${NAME}")
    endif ()

    if (LIB_FFT)
        add_executable(fft_benchmark_${NAME} ${COMMON_SRCS} ${FFT_SRCS} ${LIB_SOURCES})
        target_link_libraries(fft_benchmark_${NAME} PRIVATE ${LIB_LIBRARIES})
        target_include_directories(fft_benchmark_${NAME} PRIVATE ${COMMON_INCLUDES} ${LIB_INCLUDES})
        target_compile_definitions(fft_benchmark_${NAME} PRIVATE ${LIB_DEFINITIONS})

        add_test(
            NAME benchmark_${NAME} COMMAND fft_benchmark_${NAME} --save ${CMAKE_BINARY_DIR}/results_${NAME}.json
                                           --no-banner --no-progress ${FFT_SIZES} --complex y --inplace n --inverse n)

    endif ()
    if (LIB_SRC)
        add_executable(src_benchmark_${NAME} ${COMMON_SRCS} ${SRC_SRCS} ${LIB_SOURCES})
        target_link_libraries(src_benchmark_${NAME} PRIVATE ${LIB_LIBRARIES})
        target_include_directories(src_benchmark_${NAME} PRIVATE ${COMMON_INCLUDES} ${LIB_INCLUDES})
        target_compile_definitions(src_benchmark_${NAME} PRIVATE ${LIB_DEFINITIONS})

        add_test(
            NAME benchmark_src_${NAME} COMMAND src_benchmark_${NAME} --save
                                               ${CMAKE_BINARY_DIR}/results_src_${NAME}.json --no-banner --no-progress)
    endif ()
endfunction ()

# Enable AVX2 if on x86
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686|i386")
        add_compile_options(-mavx2 -mfma)
    endif ()
elseif (MSVC)
    if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        add_compile_options(/arch:AVX2)
    endif ()
endif ()

add_subdirectory(libs)
