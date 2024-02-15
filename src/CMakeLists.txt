cmake_minimum_required(VERSION 3.12)

set(COMMON_INCLUDES ${CMAKE_CURRENT_SOURCE_DIR})
set(COMMON_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/fft_benchmark.cpp ${CMAKE_CURRENT_SOURCE_DIR}/benchmark.hpp
                ${CMAKE_CURRENT_SOURCE_DIR}/benchmark.cpp ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp)

function (add_benchmark NAME)

    cmake_parse_arguments(LIB "" "" "SOURCES;LIBRARIES;INCLUDES" ${ARGN})

    add_executable(fft_benchmark_${NAME} ${COMMON_SRCS} ${LIB_SOURCES})
    target_link_libraries(fft_benchmark_${NAME} PUBLIC ${LIB_LIBRARIES})
    target_include_directories(fft_benchmark_${NAME} PUBLIC ${COMMON_INCLUDES} ${LIB_INCLUDES})

    install(FILES ${CMAKE_BINARY_DIR}/fft_benchmark_${NAME}
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
        OPTIONAL
    )

endfunction ()

if(KFR6)
	add_subdirectory(fft/kfr6)
else()
	message(STATUS "KFR6 benchmark test disabled.")
endif()

if(KFR5)
	add_subdirectory(fft/kfr5)
else()
	message(STATUS "KFR5 benchmark test disabled.")
endif()

if(IPP)
	add_subdirectory(fft/ipp)
else()
	message(STATUS "IPP benchmark test disabled.")
endif()

if(MKL)
	add_subdirectory(fft/mkl)
else()
	message(STATUS "MKL benchmark test disabled.")
endif()

if(FFTW)
	add_subdirectory(fft/fftw)
else()
	message(STATUS "FFTW benchmark test disabled.")
endif()
