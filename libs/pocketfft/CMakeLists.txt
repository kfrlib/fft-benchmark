# Add PocketFFT library PocketFFT is a header-only library
find_path(
    POCKETFFT_INCLUDE_DIR
    NAMES pocketfft_hdronly.h
    PATHS ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include
    NO_DEFAULT_PATH)
if (POCKETFFT_INCLUDE_DIR)
    message(STATUS "PocketFFT is found at ${POCKETFFT_INCLUDE_DIR}. Enabling PocketFFT")

    add_benchmark(pocketfft FFT SOURCES ${CMAKE_CURRENT_LIST_DIR}/pocketfft.cpp INCLUDES ${POCKETFFT_INCLUDE_DIR})
else ()
    message(STATUS "PocketFFT is not found. Please add it to your CMAKE_PREFIX_PATH.")
endif ()
