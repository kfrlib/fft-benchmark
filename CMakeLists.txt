cmake_minimum_required(VERSION 3.12)

# cmake_policy(SET CMP0091 NEW) # Forces CMake to respect MSVC_RUNTIME_LIBRARY

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

# add_compile_definitions(USE_OS_TIME=1)

if (WIN32)
    add_link_options(/DYNAMICBASE:NO)
endif ()

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

# Capture the git commit hash (with -dirty suffix if the working tree is dirty)
execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} rev-parse --short HEAD
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
execute_process(
    COMMAND git -C ${CMAKE_SOURCE_DIR} status --porcelain
    OUTPUT_VARIABLE GIT_WORKING_TREE_STATUS
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if (GIT_WORKING_TREE_STATUS)
    set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
endif ()
if (NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH "unknown")
endif ()
message(STATUS "Git commit: ${GIT_COMMIT_HASH}")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(COMMON_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/src)

set(COMMON_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/benchmark.hpp)
set(FFT_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/fft_benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/fft_benchmark.hpp
             ${CMAKE_CURRENT_LIST_DIR}/src/fft_vectors.c)
set(SRC_SRCS ${CMAKE_CURRENT_LIST_DIR}/src/src_benchmark.cpp ${CMAKE_CURRENT_LIST_DIR}/src/src_benchmark.hpp)

enable_testing()

option(OPEN "Only include open-source libraries" ON)

if (SINGLE)
    if (SINGLE MATCHES "^[A-Za-z0-9_-]+$")
        message(STATUS "SINGLE is set to ${SINGLE}: compiling only ${SINGLE} library.")
        message(STATUS "  Resulting package will have the same license as ${SINGLE}")
    else ()
        message(
            FATAL_ERROR
                "SINGLE variable contains invalid characters. It should be a simple library name without spaces or special characters."
        )
    endif ()
else ()
    if (OPEN)
        message(STATUS "OPEN is ON: compiling open-source libraries only (closed-source libraries excluded)")
        message(STATUS "  Resulting package is GPLv3-compatible")
    else ()
        message(STATUS "OPEN is OFF: compiling closed-source libraries only (open-source libraries excluded)")
    endif ()
    message(STATUS "  Note: each library has its own license")
endif ()

if (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686|i386")
    set(IS_X86 TRUE)
endif ()

if (IS_X86)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(OPTIONS_AVX2 -mavx2 -mfma)
        set(OPTIONS_AVX512 -mavx512f -mavx512dq -mavx512vl -mavx512bw -mfma)
    elseif (MSVC)
        set(OPTIONS_AVX2 /arch:AVX2)
        set(OPTIONS_AVX512 /arch:AVX512)
    endif ()
endif ()

macro (add_benchmark_target TARGET_NAME ARCH)
    add_executable(${TARGET_NAME}-${ARCH} ${COMMON_SRCS} ${ARGN} ${LIB_SOURCES})
    target_link_libraries(${TARGET_NAME}-${ARCH} PRIVATE ${LIB_LIBRARIES})
    target_include_directories(${TARGET_NAME}-${ARCH} PRIVATE ${COMMON_INCLUDES} ${LIB_INCLUDES})
    target_compile_definitions(${TARGET_NAME}-${ARCH} PRIVATE ${LIB_DEFINITIONS})
    target_compile_options(${TARGET_NAME}-${ARCH} PRIVATE ${LIB_COMPILE_OPTIONS})
    if (IS_X86)
        if (ARCH STREQUAL "avx512")
            target_compile_options(${TARGET_NAME}-${ARCH} PRIVATE ${OPTIONS_AVX512})
        else ()
            target_compile_options(${TARGET_NAME}-${ARCH} PRIVATE ${OPTIONS_AVX2})
        endif ()
    endif ()
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}-${ARCH})
    
    target_compile_definitions(${TARGET_NAME}-${ARCH} PRIVATE GIT_COMMIT_HASH="${GIT_COMMIT_HASH}")
endmacro ()

function (add_benchmark NAME)
    cmake_parse_arguments(LIB "FFT;SRC;ANY;ALL" "LICENSE" "SOURCES;LIBRARIES;INCLUDES;DEFINITIONS" ${ARGN})

    if (NOT LIB_FFT AND NOT LIB_SRC)
        message(FATAL_ERROR "Either FFT or SRC must be specified for benchmark ${NAME}")
    endif ()
    if (NOT LIB_ANY AND NOT LIB_ALL)
        message(FATAL_ERROR "Either ANY or ALL must be specified for benchmark ${NAME}")
    endif ()
    if (LIB_ANY AND LIB_ALL)
        message(FATAL_ERROR "Cannot specify both ANY and ALL for benchmark ${NAME}")
    endif ()

    if (LIB_FFT)
        add_custom_target(fft_benchmark_${NAME} ALL)
        if (NOT IS_X86 OR LIB_ANY)
            add_benchmark_target(fft_benchmark_${NAME} any ${FFT_SRCS})
            add_test(NAME benchmark_${NAME} COMMAND fft_benchmark_${NAME}-any --save
                                                    ${CMAKE_BINARY_DIR}/accuracy.${NAME}.json --accuracy)
        elseif (IS_X86 AND LIB_ALL)
            add_benchmark_target(fft_benchmark_${NAME} avx2 ${FFT_SRCS})
            add_benchmark_target(fft_benchmark_${NAME} avx512 ${FFT_SRCS})
            add_test(NAME benchmark_${NAME} COMMAND fft_benchmark_${NAME}-avx2 --save
                                                    ${CMAKE_BINARY_DIR}/accuracy.${NAME}.json --accuracy)
        endif ()

    endif ()
    if (LIB_SRC)
        add_custom_target(src_benchmark_${NAME} ALL)
        if (NOT IS_X86 OR LIB_ANY)
            add_benchmark_target(src_benchmark_${NAME} any ${SRC_SRCS})
        elseif (IS_X86 AND LIB_ALL)
            add_benchmark_target(src_benchmark_${NAME} avx2 ${SRC_SRCS})
            add_benchmark_target(src_benchmark_${NAME} avx512 ${SRC_SRCS})
        endif ()
    endif ()
endfunction ()

add_subdirectory(libs)
