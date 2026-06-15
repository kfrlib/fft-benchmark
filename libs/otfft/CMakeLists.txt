# OTFFT uses x86 SSE2/AVX/AVX2 intrinsics throughout (including _mm_free in its public header) and has no ARM/NEON path.
# Skip it entirely on non-x86 targets.
if (NOT CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686|i386")
    message(STATUS "OTFFT is not supported on ${CMAKE_SYSTEM_PROCESSOR}. Skipping.")
    return()
endif ()

include(FetchContent)

FetchContent_Declare(
    otfft
    GIT_REPOSITORY https://github.com/DEWETRON/otfft.git
    GIT_TAG v11.5.0
    GIT_SHALLOW TRUE)

# FetchContent_MakeAvailable / add_subdirectory cannot be used here: OTFFT's CMakeLists.txt calls custom macros
# (mergestaticlibs, CMakeSettings, SetFullOptimization, etc.) from its own cmake/ subfolder that are not designed for
# embedding as a subdirectory.  We just download the sources and define our own clean targets.
FetchContent_Populate(otfft)

if (otfft_POPULATED)
    message(STATUS "OTFFT is found. Enabling OTFFT")

    # Generate otfft_config.h with only AVX2 enabled. This means the factory in otfft.cpp only references OTFFT_AVX2::*
    # symbols, so we only need to build one backend library.
    set(OTFFT_WITH_SSE2)
    set(OTFFT_WITH_AVX)
    set(OTFFT_WITH_AVX2 TRUE)
    configure_file(${otfft_SOURCE_DIR}/src/otfft_config.h.in ${otfft_SOURCE_DIR}/inc/otfft_config.h)

    # algo sources must be compiled with explicit AVX2 flags so the right SIMD path is selected at compile time via
    # OTFFT_NAMESPACE=OTFFT_AVX2.
    file(GLOB OTFFT_ALGO_SOURCES ${otfft_SOURCE_DIR}/src/otfft_algo*.cpp)

    add_library(otfft_avx2 STATIC ${OTFFT_ALGO_SOURCES})
    target_include_directories(otfft_avx2 PRIVATE ${otfft_SOURCE_DIR}/inc ${otfft_SOURCE_DIR}/src)
    target_compile_definitions(otfft_avx2 PRIVATE USE_AVX2 OTFFT_NAMESPACE=OTFFT_AVX2)
    if (MSVC)
        target_compile_options(otfft_avx2 PRIVATE /arch:AVX2)
    else ()
        target_compile_options(otfft_avx2 PRIVATE -mavx2 -mfma)
    endif ()

    # Factory/dispatch code. Define only OTFFT_WITH_AVX2 so the factory only references OTFFT_AVX2::* symbols — the SSE2
    # and AVX branches are compiled out via #ifdef.
    add_library(otfft_common STATIC ${otfft_SOURCE_DIR}/src/otfft.cpp)
    target_include_directories(otfft_common PUBLIC ${otfft_SOURCE_DIR}/inc)
    target_include_directories(otfft_common PRIVATE ${otfft_SOURCE_DIR}/src)
    # No extra definitions needed — otfft_config.h already has only OTFFT_WITH_AVX2.

    add_benchmark(
        otfft
        FFT
        ALL
        SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/otfft.cpp
        LIBRARIES
        otfft_common
        otfft_avx2
        INCLUDES
        ${otfft_SOURCE_DIR}/inc)

else ()
    message(STATUS "OTFFT is not found. Disabling OTFFT")
endif ()
