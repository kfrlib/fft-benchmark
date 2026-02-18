cmake_minimum_required(VERSION 3.12)

# Add Sleef library
find_package(sleef CONFIG)
if (TARGET sleef::sleef)
    message(STATUS "Sleef ${sleef_VERSION} is found. Enabling Sleef")

    add_fft(sleef SOURCES ${CMAKE_CURRENT_LIST_DIR}/sleef.cpp DEFINITIONS SLEEF_VERSION="${sleef_VERSION}"  LIBRARIES sleef::sleef sleef::sleefdft)
else ()
    message(STATUS "sleef is not found. Please add it to your CMAKE_PREFIX_PATH.")
endif ()
