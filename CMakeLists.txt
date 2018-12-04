cmake_minimum_required(VERSION 3.0)

if (${CMAKE_GENERATOR} STREQUAL "MinGW Makefiles" OR ${CMAKE_GENERATOR} STREQUAL "MSYS Makefiles")
    if (CMAKE_BUILD_TYPE_INITIALIZED_TO_DEFAULT)
        set(CMAKE_BUILD_TYPE Release)
    endif ()
    set(CMAKE_CXX_COMPILER clang++)
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_FLAGS --target=x86_64-w64-windows-gnu CACHE STRING "compile flags" FORCE)
    set(CMAKE_C_FLAGS --target=x86_64-w64-windows-gnu CACHE STRING "compile flags" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS --target=x86_64-w64-windows-gnu)
    set(CMAKE_SHARED_LINKER_FLAGS --target=x86_64-w64-windows-gnu)
    set(CMAKE_STATIC_LINKER_FLAGS --target=x86_64-w64-windows-gnu)
endif ()

project(fft_benchmark)

include_directories(include)

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin )

add_compile_options(-fno-exceptions -fno-rtti -mllvm -x86-use-vzeroupper=0 -march=native -Wno-deprecated-declarations)
 # -march=skylake-avx512

option(ENABLE_KFR     "Enable KFR"     ON)
option(ENABLE_KISSFFT "Enable KissFFT" ON)
option(ENABLE_FFTW    "Enable FFTW"    ON)
option(ENABLE_IPP     "Enable IPP"     ON)

link_directories(bin)
link_directories(lib)

if (ENABLE_KFR)
    set(DFT_SRC
            include/kfr/dft/impl/dft-impl-f32.cpp
            include/kfr/dft/impl/dft-impl-f64.cpp)
    #set(DFT_SRC )
    add_executable(fft_benchmark_kfr_double fft_benchmark.cpp benchmark.hpp kfr.hpp ${DFT_SRC})
    target_compile_definitions(fft_benchmark_kfr_double PRIVATE FFT_TEST="kfr.hpp")
    target_compile_options(fft_benchmark_kfr_double PRIVATE -std=c++1y)
    add_executable(fft_benchmark_kfr_float fft_benchmark.cpp benchmark.hpp kfr.hpp ${DFT_SRC})
    target_compile_definitions(fft_benchmark_kfr_float PRIVATE FFT_TEST="kfr.hpp" TYPE_FLOAT=1)
    target_compile_options(fft_benchmark_kfr_float PRIVATE -std=c++1y)
endif ()

if (ENABLE_KISSFFT)
    add_executable(fft_benchmark_kissfft_double fft_benchmark.cpp benchmark.hpp kissfft.hpp kissfft/kiss_fft.c)
    target_compile_definitions(fft_benchmark_kissfft_double PRIVATE FFT_TEST="kissfft.hpp")
    add_executable(fft_benchmark_kissfft_float fft_benchmark.cpp benchmark.hpp kissfft.hpp kissfft/kiss_fft.c)
    target_compile_definitions(fft_benchmark_kissfft_float PRIVATE FFT_TEST="kissfft.hpp" TYPE_FLOAT=1)
endif ()

if (ENABLE_FFTW)
    add_executable(fft_benchmark_fftw_double fft_benchmark.cpp benchmark.hpp fftw.hpp)
    target_compile_definitions(fft_benchmark_fftw_double PRIVATE FFT_TEST="fftw.hpp")
    add_executable(fft_benchmark_fftw_float fft_benchmark.cpp benchmark.hpp fftw.hpp)
    target_compile_definitions(fft_benchmark_fftw_float PRIVATE FFT_TEST="fftw.hpp" TYPE_FLOAT=1)

    target_link_libraries(fft_benchmark_fftw_double fftw3-3)
    target_link_libraries(fft_benchmark_fftw_float fftw3f-3)
endif ()

if (ENABLE_IPP)
    add_executable(fft_benchmark_ipp_double fft_benchmark.cpp benchmark.hpp ipp.hpp)
    target_compile_definitions(fft_benchmark_ipp_double PRIVATE FFT_TEST="ipp.hpp")
    add_executable(fft_benchmark_ipp_float fft_benchmark.cpp benchmark.hpp ipp.hpp)
    target_compile_definitions(fft_benchmark_ipp_float PRIVATE FFT_TEST="ipp.hpp" TYPE_FLOAT=1)

    target_link_libraries(fft_benchmark_ipp_double ippsmt ippcoremt ippvmmt)
    target_link_libraries(fft_benchmark_ipp_float ippsmt ippcoremt ippvmmt)
endif ()

