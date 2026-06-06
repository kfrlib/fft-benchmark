if (KFR_SRC_DIR)

    add_compile_definitions(KFR_NG=1)

    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_EXTENSIONS ON)

    set(KFR_ENABLE_MULTIARCH
        FALSE
        CACHE BOOL "" FORCE)
    add_subdirectory(${KFR_SRC_DIR} kfr)

    message(STATUS "KFR_SRC_DIR is set. Enabling KFR")

else ()
    find_package(KFR CONFIG)
    if (KFR_FOUND)
        message(STATUS "KFR ${KFR_VERSION} is found. Enabling KFR")
    else ()
        message(STATUS "KFR is not found. Disabling KFR Add <KFR_ROOT>/lib/cmake to CMAKE_PREFIX_PATH")
    endif ()
endif ()

if (TARGET kfr)
    add_benchmark(kfr FFT SRC SOURCES ${CMAKE_CURRENT_LIST_DIR}/kfr.cpp LIBRARIES kfr kfr_dft kfr_dsp)
endif ()
