cmake_minimum_required(VERSION 3.12)

find_package(kissfft CONFIG)

if (kissfft_FOUND)
    message(STATUS "kissfft ${kissfft_VERSION} is found. Enabling kissfft")

    add_fft(kissfft SOURCES ${CMAKE_CURRENT_LIST_DIR}/kissfft.cpp DEFINITIONS KISSFFT_VERSION="${kissfft_VERSION}" LIBRARIES kissfft::kissfft-float)

else ()
    message(STATUS "kissfft is not found. Disabling kissfft.")
endif ()
