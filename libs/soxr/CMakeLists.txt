find_library(SOXR_LIBRARY soxr)
find_path(SOXR_INCLUDE_DIR soxr.h)

if (SOXR_LIBRARY AND SOXR_INCLUDE_DIR)
    message(STATUS "soxr is found. Enabling soxr")

    add_benchmark(soxr SRC ALL SOURCES ${CMAKE_CURRENT_LIST_DIR}/soxr.cpp LIBRARIES ${SOXR_LIBRARY} INCLUDES ${SOXR_INCLUDE_DIR})

else ()
    message(STATUS "soxr is not found. Disabling soxr")
endif ()
