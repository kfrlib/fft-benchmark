cmake_minimum_required(VERSION 3.12)

find_package(FFTW3)
if (FFTW3_FOUND)

    add_benchmark(fftw
        SOURCES 
            ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp 
        LIBRARIES 
            ${FFTW3_LIBRARIES} 
        INCLUDES 
            ${FFTW3_INCLUDE_DIRS}
	VERSION
	    ${FFTW3_VERSION}
    )

endif ()
