function(find_package_standard)

    # Parse expected arguments and extract expected library headers 
    set(options "HEADER_FILE_ONLY")
    set(oneValueArgs "")
    set(multiValueArgs NAMES HEADERS HINTS PATHS PATH_SUFFIXES)

    cmake_parse_arguments(LIBRARY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Extract package name from basename
    list(LENGTH LIBRARY_UNPARSED_ARGUMENTS ARGC)
    if(ARGC GREATER 0)
        list(GET LIBRARY_UNPARSED_ARGUMENTS 0 LIBRARY)
    else()
        get_filename_component(FILE_NAME ${CMAKE_CURRENT_LIST_FILE} NAME_WE)
        if(${FILE_NAME} MATCHES "^Find(.+)$")
            set(LIBRARY ${CMAKE_MATCH_1})
        else()
            message(WARNING "Library name cannot be extracted from `${CMAKE_CURRENT_LIST_FILE}`.")
        endif()
    endif()

    if (${LIBRARY}_FOUND)
        return()
    endif()

    # Hints for default system directory paths for libraries and includes (Linux specific)
    if (UNIX AND NOT APPLE)
        list(APPEND CMAKE_SYSTEM_LIBRARY_PATH /usr/lib /usr/local/lib)
        list(APPEND CMAKE_SYSTEM_INCLUDE_PATH /usr/include /usr/local/include)
    endif()

    # Prepare include and library file location variables
    set(_INCLUDE_PATHS)
    set(_LIBRARY_PATHS)
    foreach(LIBRARY_PATH ${LIBRARY_PATHS})
        list(APPEND _LIBRARY_PATHS "${LIBRARY_PATH}/lib")
        list(APPEND _INCLUDE_PATHS "${LIBRARY_PATH}/include")
    endforeach()

    # Merge with LD_LIBRARY_PATH and DYLD_LIBRARY_PATH (lower priority)
    if (DEFINED ENV{LD_LIBRARY_PATH})
        string(REPLACE ":" ";" LD_LIBRARY_PATH_LIST $ENV{LD_LIBRARY_PATH})
        list(APPEND _LIBRARY_PATHS ${LD_LIBRARY_PATH_LIST})
    endif()

    if (DEFINED ENV{DYLD_LIBRARY_PATH})
        string(REPLACE ":" ";" DYLD_LIBRARY_PATH_LIST $ENV{DYLD_LIBRARY_PATH})
        list(APPEND _LIBRARY_PATHS ${DYLD_LIBRARY_PATH_LIST})
    endif()

    list(REMOVE_DUPLICATES _LIBRARY_PATHS)
    list(REMOVE_DUPLICATES _INCLUDE_PATHS)

    # Looking for library headers
    if(LIBRARY_HEADERS)

        find_path(${LIBRARY}_INCLUDE_DIRS
            NAMES 
                ${LIBRARY_HEADERS}
            PATHS 
                ${${LIBRARY}_INCLUDE_DIR} ${CMAKE_SYSTEM_INCLUDE_PATH} ${_INCLUDE_PATHS}
            PATH_SUFFIXES 
                include
            HINTS 
                ${LIBRARY_HINTS}
        )

    endif()

    # Looking for library version
    set(${LIBRARY}_VERSION "0.0.0")
    foreach(LIBRARY_HEADER ${LIBRARY_HEADERS})

        set(LIBRARY_HEADER "${${LIBRARY}_INCLUDE_DIRS}/${LIBRARY_HEADER}")
        if("${${LIBRARY}_VERSION}" STREQUAL "0.0.0" AND EXISTS ${LIBRARY_HEADER})

            file(READ "${LIBRARY_HEADER}" LIBRARY_HEADER_CONTENTS)

            string(REGEX MATCH "#define [^ ]*VERSION_MAJOR ([0-9]+)" VERSION_MAJOR_MATCH "${LIBRARY_HEADER_CONTENTS}")
            if(VERSION_MAJOR_MATCH)
                set(${LIBRARY}_VERSION_MAJOR ${CMAKE_MATCH_1})
            else()
                set(${LIBRARY}_VERSION_MAJOR 0)
            endif()

            string(REGEX MATCH "#define [^ ]*VERSION_MINOR ([0-9]+)" VERSION_MINOR_MATCH "${LIBRARY_HEADER_CONTENTS}")
            if(VERSION_MINOR_MATCH)
                set(${LIBRARY}_VERSION_MINOR ${CMAKE_MATCH_1})
            else()
                set(${LIBRARY}_VERSION_MINOR 0)
            endif()
            
            string(REGEX MATCH "#define [^ ]*VERSION_PATCH ([0-9]+)" VERSION_PATCH_MATCH "${LIBRARY_HEADER_CONTENTS}")
            if(VERSION_PATCH_MATCH)
                set(${LIBRARY}_VERSION_PATCH ${CMAKE_MATCH_1})
            else()
                set(${LIBRARY}_VERSION_PATCH 0)
            endif()

            string(REGEX MATCH "#define [^ ]*VERSION_TWEAK ([0-9]+)" VERSION_TWEAK_MATCH "${LIBRARY_HEADER_CONTENTS}")
            if(VERSION_TWEAK_MATCH)
                set(${LIBRARY}_VERSION_TWEAK ${CMAKE_MATCH_1})
            else()
                set(${LIBRARY}_VERSION_TWEAK 0)
            endif()

            string(REGEX MATCH "#define [^ ]*VERSION_COUNT([0-9]+)" VERSION_COUNT_MATCH "${LIBRARY_HEADER_CONTENTS}")
            if(VERSION_COUNT_MATCH)
                set(${LIBRARY}_VERSION_COUNT ${CMAKE_MATCH_1})
            else()
                set(${LIBRARY}_VERSION_COUNT 0)
            endif()
            if(${LIBRARY}_VERSION_COUNT GREATER 4)
                set(${LIBRARY}_VERSION_COUNT 4)
            endif()

            string(REGEX MATCH "#define [^ ]*VERSION ([0-9]+)" VERSION_MATCH "${LIBRARY_HEADER_CONTENTS}")
            if(VERSION_MATCH)
                set(${LIBRARY}_VERSION ${CMAKE_MATCH_1})
            else()
                set(${LIBRARY}_VERSION "${${LIBRARY}_VERSION_MAJOR}.${${LIBRARY}_VERSION_MINOR}.${${LIBRARY}_VERSION_PATCH}")
            endif()

        endif()

    endforeach()

    if("${${LIBRARY}_VERSION}" STREQUAL "0.0.0")
        set(${LIBRARY}_VERSION "")
    endif()

    # Look for libraries
    if(NOT LIBRARY_HEADER_FILE_ONLY)

        if(NOT DEFINED ${LIBRARY}_LIBRARIES)
            
            if(NOT LIBRARY_NAMES)
                message(FATAL_ERROR "Missing library name looking for package `${LIBRARY}`")
            endif()

            foreach(LIBRARY_NAME IN LISTS LIBRARY_NAMES)

                string(REGEX REPLACE "^lib" "" STRIPLIB "${LIBRARY_NAME}")
            
                string(TOLOWER ${STRIPLIB} LOWER_STRIPLIB)
                string(TOUPPER ${STRIPLIB} UPPER_STRIPLIB)
                
                string(TOLOWER ${LIBRARY_NAME} LOWER_LIBRARY)
                string(TOUPPER ${LIBRARY_NAME} UPPER_LIBRARY)

                if(TARGET ${${LIBRARY_NAME}_LIBRARY})

                    set(${LIBRARY_NAME}_LIBRARY ${CMAKE_BINARY_DIR}/lib/lib${LIBRARY_NAME}.so)
                    if(APPLE)
                        set(${LIBRARY_NAME}_LIBRARY ${CMAKE_BINARY_DIR}/lib/lib${LIBRARY_NAME}.dylib)
                    endif()

                else()

                    find_library(
                        ${LIBRARY_NAME}_LIBRARY
                        NAMES ${LIBRARY_NAME} ${LOWER_LIBRARY}  ${UPPER_LIBRARY} 
                            ${STRIPLIB}     ${LOWER_STRIPLIB} ${UPPER_STRIPLIB}
                        PATHS ${_LIBRARY_PATHS}
                        PATH_SUFFIXES lib lib64
                    )
                    
                endif()

                if(${LIBRARY_NAME}_LIBRARY)
                    list(APPEND ${LIBRARY}_LIBRARIES ${${LIBRARY_NAME}_LIBRARY})
                    unset(${LIBRARY_NAME}_LIBRARY CACHE)
                endif()


            endforeach()

            if(TARGET ${LIBRARY})

                set(${LIBRARY}_FOUND TRUE)
                set(${LIBRARY}_INCLUDE_DIRS ${CMAKE_BINARY_DIR}/include)

            else()

                # Provide information about how to use the library
                include(FindPackageHandleStandardArgs)
                if(NOT ${LIBRARY}_VERSION) 
                    find_package_handle_standard_args(${LIBRARY} 
                        FOUND_VAR ${LIBRARY}_FOUND
                        REQUIRED_VARS ${LIBRARY}_LIBRARIES ${LIBRARY}_INCLUDE_DIRS
                    )
                else()
                    find_package_handle_standard_args(${LIBRARY} 
                        FOUND_VAR ${LIBRARY}_FOUND
                        REQUIRED_VARS ${LIBRARY}_LIBRARIES ${LIBRARY}_INCLUDE_DIRS
                        VERSION_VAR "${LIBRARY}_VERSION"
                        HANDLE_VERSION_RANGE
                    )
                endif()                
            endif()

        endif()

    elseif(${LIBRARY}_INCLUDE_DIRS)
        
        set(${LIBRARY}_FOUND TRUE)

    endif()

    # Pass the variables back to the parent scope
    set(${LIBRARY}_FOUND ${${LIBRARY}_FOUND} PARENT_SCOPE)
    set(${LIBRARY}_LIBRARIES ${${LIBRARY}_LIBRARIES} PARENT_SCOPE)
    set(${LIBRARY}_INCLUDE_DIRS ${${LIBRARY}_INCLUDE_DIRS} PARENT_SCOPE)
    set(${LIBRARY}_VERSION ${${LIBRARY}_VERSION} PARENT_SCOPE)
    set(${LIBRARY}_VERSION_MAJOR ${${LIBRARY}_VERSION_MAJOR} PARENT_SCOPE)
    set(${LIBRARY}_VERSION_MINOR ${${LIBRARY}_VERSION_MINOR} PARENT_SCOPE)
    set(${LIBRARY}_VERSION_PATCH ${${LIBRARY}_VERSION_PATCH} PARENT_SCOPE)
    set(${LIBRARY}_VERSION_TWEAK ${${LIBRARY}_VERSION_TWEAK} PARENT_SCOPE)
    set(${LIBRARY}_VERSION_COUNT ${${LIBRARY}_VERSION_COUNT} PARENT_SCOPE)
    
    # Remove cache variables
    if(DEFINED ${LIBRARY}_LIBRARIES)
        unset(${LIBRARY}_LIBRARIES CACHE)
    endif()

    if(DEFINED ${LIBRARY}_INCLUDE_DIRS)
        unset(${LIBRARY}_INCLUDE_DIRS CACHE)
    endif()

    if(DEFINED ${LIBRARY}_INCLUDE_DIR)
        unset(${LIBRARY}_INCLUDE_DIR CACHE)
    endif()

endfunction()

macro(target_link_package)

    # Parse expected arguments
    set(options EXACT QUIET REQUIRED CONFIG NO_MODULE GLOBAL NO_POLICY_SCOPE BYPASS_PROVIDER
                NO_DEFAULT_PATH NO_PACKAGE_ROOT_PATH NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH
                NO_SYSTEM_ENVIRONMENT_PATH NO_CMAKE_PACKAGE_REGISTRY NO_CMAKE_BUILDS_PATH
                NO_CMAKE_SYSTEM_PATH NO_CMAKE_INSTALL_PREFIX NO_CMAKE_SYSTEM_PACKAGE_REGISTRY
                CMAKE_FIND_ROOT_PATH_BOTH ONLY_CMAKE_FIND_ROOT_PATH NO_CMAKE_FIND_ROOT_PATH)

    set(oneValueArgs NAMES REGISTRY_VIEW DESTINATION RENAME)

    set(multiValueArgs COMPONENTS OPTIONAL_COMPONENTS CONFIGS HINTS PATHS PATH_SUFFIXES TARGETS CONFIGURATIONS)

    cmake_parse_arguments(MY "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Check mandatory and optional unparsed aguments
    list(LENGTH MY_UNPARSED_ARGUMENTS ARGC)
    if(ARGC LESS 1)
        message(FATAL_ERROR "target_link_package: Please provide at least TARGET_NAME and PACKAGE_NAME.")
    elseif(ARGC LESS 2)
        message(FATAL_ERROR "target_link_package: Please provide PACKAGE_NAME.")
    else()
    
        list(GET MY_UNPARSED_ARGUMENTS 0 MY_TARGET_NAME)
        list(GET MY_UNPARSED_ARGUMENTS 1 MY_PACKAGE_NAME)
        if(ARGC GREATER 2)
            list(GET MY_UNPARSED_ARGUMENTS 2 MY_VERSION)
        endif()
    endif()

    # Prepare find_package arguments
    set(ARGS)
    if(MY_EXACT)
        list(APPEND ARGS EXACT)
    endif()
    if(MY_QUIET)
        list(APPEND ARGS QUIET)
    endif()
    if(MY_REQUIRED)
        list(APPEND ARGS REQUIRED ${ENDIF_MY_REQUIRED})
    endif()
    if(MY_CONFIG)
        list(APPEND ARGS CONFIG ${ENDIF_MY_CONFIG})
    endif()
    if(MY_NO_MODULE)
        list(APPEND ARGS NO_MODULE ${ENDIF_MY_NO_MODULE})
    endif()
    if(MY_GLOBAL)
        list(APPEND ARGS GLOBAL ${ENDIF_MY_GLOBAL})
    endif()
    if(MY_NO_POLICY_SCOPE)
        list(APPEND ARGS NO_POLICY_SCOPE ${ENDIF_MY_NO_POLICY_SCOPE})
    endif()
    if(MY_BYPASS_PROVIDER)
        list(APPEND ARGS BYPASS_PROVIDER ${ENDIF_MY_BYPASS_PROVIDER})
    endif()
    if(MY_NO_DEFAULT_PATH)
        list(APPEND ARGS NO_DEFAULT_PATH ${ENDIF_MY_NO_DEFAULT_PATH})
    endif()
    if(MY_NO_PACKAGE_ROOT_PATH)
        list(APPEND ARGS NO_PACKAGE_ROOT_PATH ${ENDIF_MY_NO_PACKAGE_ROOT_PATH})
    endif()
    if(MY_NO_CMAKE_PATH)
        list(APPEND ARGS NO_CMAKE_PATH ${ENDIF_MY_NO_CMAKE_PATH})
    endif()
    if(MY_NO_CMAKE_ENVIRONMENT_PATH)
        list(APPEND ARGS NO_CMAKE_ENVIRONMENT_PATH ${ENDIF_MY_NO_CMAKE_ENVIRONMENT_PATH})
    endif()
    if(MY_NO_SYSTEM_ENVIRONMENT_PATH)
        list(APPEND ARGS NO_SYSTEM_ENVIRONMENT_PATH ${ENDIF_MY_NO_SYSTEM_ENVIRONMENT_PATH})
    endif()
    if(MY_NO_CMAKE_PACKAGE_REGISTRY)
        list(APPEND ARGS NO_CMAKE_PACKAGE_REGISTRY ${ENDIF_MY_NO_CMAKE_PACKAGE_REGISTRY})
    endif()
    if(MY_NO_CMAKE_BUILDS_PATH)
        list(APPEND ARGS NO_CMAKE_BUILDS_PATH ${ENDIF_MY_NO_CMAKE_BUILDS_PATH})
    endif()
    if(MY_NO_CMAKE_SYSTEM_PATH)
        list(APPEND ARGS NO_CMAKE_SYSTEM_PATH ${ENDIF_MY_NO_CMAKE_SYSTEM_PATH})
    endif()
    if(MY_NO_CMAKE_INSTALL_PREFIX)
        list(APPEND ARGS NO_CMAKE_INSTALL_PREFIX ${ENDIF_MY_NO_CMAKE_INSTALL_PREFIX})
    endif()
    if(MY_NO_CMAKE_SYSTEM_PACKAGE_REGISTRY)
        list(APPEND ARGS NO_CMAKE_SYSTEM_PACKAGE_REGISTRY ${ENDIF_MY_NO_CMAKE_SYSTEM_PACKAGE_REGISTRY})
    endif()
    if(MY_CMAKE_FIND_ROOT_PATH_BOTH)
        list(APPEND ARGS CMAKE_FIND_ROOT_PATH_BOTH ${ENDIF_MY_CMAKE_FIND_ROOT_PATH_BOTH})
    endif()
    if(MY_ONLY_CMAKE_FIND_ROOT_PATH)
        list(APPEND ARGS ONLY_CMAKE_FIND_ROOT_PATH ${ENDIF_MY_ONLY_CMAKE_FIND_ROOT_PATH})
    endif()
    if(MY_NO_CMAKE_FIND_ROOT_PATH)
        list(APPEND ARGS NO_CMAKE_FIND_ROOT_PATH ${ENDIF_MY_NO_CMAKE_FIND_ROOT_PATH})
    endif()

    if(MY_COMPONENTS)
        list(APPEND ARGS COMPONENTS ${MY_COMPONENTS})
    endif()
    if(MY_OPTIONAL_COMPONENTS)
        list(APPEND ARGS OPTIONAL_COMPONENTS ${MY_OPTIONAL_COMPONENTS})
    endif()
    if(MY_CONFIGS)
        list(APPEND ARGS CONFIGS ${MY_CONFIGS})
    endif()
    if(MY_HINTS)
        list(APPEND ARGS HINTS ${MY_HINTS})
    endif()
    if(MY_PATHS)
        list(APPEND ARGS PATHS ${MY_PATHS})
    endif()
    if(MY_PATH_SUFFIXES)
        list(APPEND ARGS PATH_SUFFIXES ${MY_PATH_SUFFIXES})
    endif()
    if(MY_TARGETS)
        list(APPEND ARGS TARGETS ${MY_TARGETS})
    endif()
    if(MY_CONFIGURATIONS)
        list(APPEND ARGS CONFIGURATIONS ${MY_CONFIGURATIONS})
    endif()

    if(NOT "${MY_REGISTRY_VIEW}" STREQUAL "")
        list(APPEND ARGS REGISTRY_VIEW "${MY_REGISTRY_VIEW}")
    endif()
    if(NOT "${MY_DESTINATION}" STREQUAL "")
        list(APPEND ARGS DESTINATION "${MY_DESTINATION}")
    endif()
    if(NOT "${MY_RENAME}" STREQUAL "")
        list(APPEND ARGS RENAME "${MY_RENAME}")
    endif()

    # Call find package
    if(TARGET ${MY_PACKAGE_NAME})

        target_link_libraries(${MY_TARGET_NAME} ${MY_PACKAGE_NAME})
        if(APPLE) # Set the rpath for the test executable to find the library at runtime
            set_target_properties(${MY_TARGET_NAME} PROPERTIES
                BUILD_WITH_INSTALL_RPATH TRUE
                INSTALL_RPATH "@executable_path/../${CMAKE_INSTALL_LIBDIR}")
        endif()

    else()
        
        # Load library if not found
        if (NOT ${MY_PACKAGE_NAME}_FOUND)
            find_package(${MY_PACKAGE_NAME} ${ARGS})
        endif()

        # Make connection between target and package
        if (${MY_PACKAGE_NAME}_FOUND)
            target_link_libraries(${MY_TARGET_NAME} ${${MY_PACKAGE_NAME}_LIBRARIES})
            target_include_directories(${MY_TARGET_NAME} PUBLIC ${${MY_PACKAGE_NAME}_INCLUDE_DIRS})
        endif()
    endif()

endmacro()
