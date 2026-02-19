cmake_minimum_required(VERSION 3.12)

message(STATUS "Using bundled kissfft ${KISSFFT_VERSION}")

add_fft(kissfft SOURCES ${CMAKE_CURRENT_LIST_DIR}/kissfft.cpp DEFINITIONS KISSFFT_VERSION="${KISSFFT_VERSION}")
