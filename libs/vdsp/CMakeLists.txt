if (APPLE)
    message(STATUS "Apple platform detected. Enabling vDSP (Accelerate)")

    add_fft(
        vdsp
        SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/vdsp.cpp
        LIBRARIES
        "-framework Accelerate"
    )
endif ()
