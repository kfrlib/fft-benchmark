find_package(SampleRate CONFIG)

if (SampleRate_FOUND)
    message(STATUS "libsamplerate is found. Enabling libsamplerate")

    add_benchmark(libsamplerate SRC SOURCES ${CMAKE_CURRENT_LIST_DIR}/libsamplerate.cpp LIBRARIES
                  SampleRate::samplerate)

else ()
    message(STATUS "libsamplerate is not found. Disabling libsamplerate")
endif ()
