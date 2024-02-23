# Check if the source directory is the same as the binary directory
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    # Display a fatal error message if it's an in-source build
    message(FATAL_ERROR "In-source build not allowed. Please make a new directory (called a build directory) and run the cmake command from there (don't forget now to remove the cached variables).")
endif()