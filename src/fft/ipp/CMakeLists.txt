cmake_minimum_required(VERSION 3.12)

find_package(IPP)
if (IPP_FOUND)

    add_benchmark(ipp 
        SOURCES 
            ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp 
        LIBRARIES 
            ${IPP_LIBRARIES}
        INCLUDES 
            ${IPP_INCLUDE_DIRS}
        VERSION
            ${IPP_VERSION}
    )

endif ()
