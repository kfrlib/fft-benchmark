cmake_minimum_required(VERSION 3.12)

find_package(KFR REQUIRED)

if (KFR_FOUND)
    message(STATUS "KFR ${KFR_VERSION} is found. Enabling KFR")

    include_directories(${KFR_INCLUDE_DIRS})
    add_fft(
        kfr
        SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/kfr.cpp
        LIBRARIES
        ${KFR_LIBRARIES})
else ()
    
    if (KFR_DIR)
        set(KFR_ARCH avx2 CACHE STRING "" FORCE)
        set(KFR_ENABLE_MULTIARCH FALSE CACHE BOOL "" FORCE)
        add_subdirectory(${KFR_DIR} kfr)

        message(STATUS "KFR_DIR is set. Enabling KFR")

        add_fft(
            kfr
            SOURCES
            ${CMAKE_CURRENT_LIST_DIR}/kfr.cpp
            LIBRARIES
            ${KFR_LIBRARIES})

    else ()
        message(STATUS "KFR is not found. Disabling KFR Add <KFR_ROOT>/lib/cmake to CMAKE_PREFIX_PATH")
    endif ()
endif ()
