# Add PFFFT library
find_package(pffft CONFIG)
if (TARGET pffft::pffft)
    message(STATUS "PFFFT is found. Enabling PFFFT")

    add_benchmark(pffft FFT SOURCES ${CMAKE_CURRENT_LIST_DIR}/pffft.cpp LIBRARIES pffft::pffft)
else ()
    message(STATUS "PFFFT is not found. Please add it to your CMAKE_PREFIX_PATH.")
endif ()
