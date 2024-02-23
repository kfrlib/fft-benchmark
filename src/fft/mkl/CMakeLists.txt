cmake_minimum_required(VERSION 3.12)

set(MKL_THREADING sequential CACHE STRING "" FORCE)
set(MKL_LINK static CACHE STRING "" FORCE)

find_package(MKL)

if (MKL_FOUND)

    add_benchmark(mkl
        SOURCES
            ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp 
        LIBRARIES
            ${MKL_LIBRARIES}
        INCLUDES
            ${MKL_INCLUDE_DIRS}
        VERSION
            ${MKL_VERSION}
    )

endif ()
