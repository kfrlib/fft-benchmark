cmake_minimum_required(VERSION 3.12)

find_package(IPP CONFIG)

if (IPP_FOUND AND IPP_ipps_FOUND)
    message(STATUS "IPP ${IPP_VERSION} is found. Enabling IPP")

    add_fft(ipp SOURCES ${CMAKE_CURRENT_LIST_DIR}/ipp.cpp LIBRARIES IPP::ipps)

else ()
    message(STATUS "IPP is not found. Disabling IPP. Add <IPP_ROOT>/lib/cmake to CMAKE_PREFIX_PATH")
endif ()
