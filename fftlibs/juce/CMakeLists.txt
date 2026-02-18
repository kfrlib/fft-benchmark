cmake_minimum_required(VERSION 3.12)

find_package(JUCE CONFIG)

if (TARGET juce::juce_dsp)
    # Print juce version
    message(STATUS "JUCE ${JUCE_VERSION} is found. Enabling JUCE")    

    add_fft(juce SOURCES ${CMAKE_CURRENT_LIST_DIR}/juce.cpp LIBRARIES juce::juce_dsp)

else ()
    message(STATUS "JUCE is not found. Please add it to your CMAKE_PREFIX_PATH.")
endif ()

