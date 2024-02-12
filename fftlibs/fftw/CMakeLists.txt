cmake_minimum_required(VERSION 3.12)

find_package(FFTW3)
find_package(FFTW3f)

if (FFTW3_FOUND AND FFTW3f_FOUND)
    message(STATUS "FFTW3 ${FFTW3_VERSION} is found. Enabling FFTW3")

    add_fft(fftw SOURCES ${CMAKE_CURRENT_LIST_DIR}/fftw.cpp LIBRARIES FFTW3::fftw3 FFTW3::fftw3f)

else ()
    message(STATUS "FFTW3 is not found. Disabling FFTW3. Add <FFTW3> to CMAKE_PREFIX_PATH")
endif ()
