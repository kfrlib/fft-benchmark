cmake_minimum_required(VERSION 3.12)

set(MKL_THREADING sequential CACHE STRING "" FORCE)
set(MKL_LINK static CACHE STRING "" FORCE)

find_package(MKL CONFIG)

if (MKL_FOUND)
    message(STATUS "MKL ${MKL_VERSION} is found. Enabling MKL")

    add_fft(mkl SOURCES ${CMAKE_CURRENT_LIST_DIR}/mkl.cpp LIBRARIES MKL::MKL)

else ()
    message(STATUS "MKL is not found. Disabling MKL. Add <MKL_ROOT>/lib/cmake to CMAKE_PREFIX_PATH")
endif ()
