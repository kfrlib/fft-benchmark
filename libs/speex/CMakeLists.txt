find_library(SPEEXDSP_LIBRARY speexdsp)
find_path(SPEEXDSP_INCLUDE_DIR speex/speex_resampler.h)

if (SPEEXDSP_LIBRARY AND SPEEXDSP_INCLUDE_DIR)
    message(STATUS "speexdsp is found. Enabling speex")

    add_benchmark(speex SRC ALL SOURCES ${CMAKE_CURRENT_LIST_DIR}/speex.cpp LIBRARIES ${SPEEXDSP_LIBRARY} INCLUDES ${SPEEXDSP_INCLUDE_DIR})

else ()
    message(STATUS "speexdsp is not found. Disabling speex")
endif ()
