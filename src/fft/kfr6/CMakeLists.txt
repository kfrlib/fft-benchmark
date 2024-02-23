cmake_minimum_required(VERSION 3.12)

set(KFR "${KFR6}")
find_package(KFR 6 REQUIRED)

if (KFR_FOUND)

    add_benchmark( kfr6
        SOURCES 
            ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp 
        LIBRARIES 
            ${KFR_LIBRARIES}
        INCLUDES
            ${KFR_INCLUDE_DIRS}
        VERSION
            ${KFR_VERSION}
    )

endif ()
