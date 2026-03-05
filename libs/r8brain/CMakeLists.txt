include(FetchContent)

FetchContent_Declare(
    r8brain_free_src
    GIT_REPOSITORY https://github.com/avaneev/r8brain-free-src.git
    GIT_TAG master
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(r8brain_free_src)

if (r8brain_free_src_POPULATED)
    message(STATUS "r8brain-free-src is found. Enabling r8brain")

    add_library(r8brain_lib STATIC ${r8brain_free_src_SOURCE_DIR}/fft/pffft_double.c)
    target_include_directories(r8brain_lib PUBLIC ${r8brain_free_src_SOURCE_DIR})
    target_compile_definitions(r8brain_lib PUBLIC R8B_PFFFT_DOUBLE=1)

    add_benchmark(r8brain SRC SOURCES ${CMAKE_CURRENT_LIST_DIR}/r8brain.cpp LIBRARIES r8brain_lib)

else ()
    message(STATUS "r8brain-free-src is not found. Disabling r8brain")
endif ()
