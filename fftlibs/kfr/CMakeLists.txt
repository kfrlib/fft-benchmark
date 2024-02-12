cmake_minimum_required(VERSION 3.12)

find_package(KFR CONFIG)
if (KFR_FOUND)
    message(STATUS "KFR ${KFR_VERSION} is found. Enabling KFR")

    add_fft(
        kfr
        SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/kfr.cpp
        LIBRARIES
        kfr
        kfr_dft)
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
            kfr
            kfr_dft)

    else ()
        message(STATUS "KFR is not found. Disabling KFR Add <KFR_ROOT>/lib/cmake to CMAKE_PREFIX_PATH")
    endif ()
endif ()
