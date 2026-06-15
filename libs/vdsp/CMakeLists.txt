if (APPLE)
    message(STATUS "Apple platform detected. Enabling vDSP (Accelerate)")

    add_benchmark(vdsp FFT ANY SOURCES ${CMAKE_CURRENT_LIST_DIR}/vdsp.cpp LIBRARIES "-framework Accelerate")
endif ()
