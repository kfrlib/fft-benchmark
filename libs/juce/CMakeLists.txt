find_package(JUCE CONFIG)

if (TARGET juce::juce_dsp)
    # Print juce version
    message(STATUS "JUCE ${JUCE_VERSION} is found. Enabling JUCE")

    add_benchmark(
        juce
        FFT
        SRC
        ANY
        SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/juce.cpp
        LIBRARIES
        juce::juce_dsp)

else ()
    message(STATUS "JUCE is not found. Please add it to your CMAKE_PREFIX_PATH.")
endif ()
