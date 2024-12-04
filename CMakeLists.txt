cmake_minimum_required(VERSION 3.16)

# Compiling the VKI static lib.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

set(SOURCE VKInterface.cpp)

# Set build type to Debug unless release is specified.
if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message("-- Configure VKInterface CMAKE_BUILD_TYPE to Release.")
	add_compile_options( -O2 )
else ()
    message("-- Configure VKInterface CMAKE_BUILD_TYPE to Debug.")
	set(CMAKE_BUILD_TYPE Debug)
	add_compile_options( -O0 -g )
    add_compile_definitions( VKI_ENABLE_VULKAN_VALIDATION_LAYERS )
endif()

if (FALSE)#${BUILD_SHARED_LIBS}) // TODO later.
    message("-- Configure VKInterface shared library type.")
    add_library(VKInterface SHARED ${SOURCE})
else()
    message("-- Configure VKInterface static library type.")
    add_library(VKInterface STATIC ${SOURCE})
endif()
target_include_directories(VKInterface INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

add_compile_options( -Wall -Wextra -Wpedantic )

# Here is the linking with the GMP static lib.
add_library(GMP STATIC IMPORTED) # C lib required for the GMP C++ lib.
set_target_properties(GMP PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libgmp/libgmp.a")

add_library(GMPXX STATIC IMPORTED) # C++ encapsulation of GMP.
set_target_properties(GMPXX PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libgmp/libgmpxx.a")

target_link_libraries(VKInterface PRIVATE GMP PRIVATE GMPXX)

# Building documentation :

if (${VKI_BUILD_DOCUMENTATION})
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/docs/")
endif()

# Building exemples :

if (${VKI_BUILD_EXEMPLE_EXECUTABLES})

    message("-- Building exemples.")

    set(EXEMPLE_SOURCE
        "simple_triangle"
    )

    foreach (SOURCE_NAME IN LISTS EXEMPLE_SOURCE)
        add_executable(${SOURCE_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/exemples/${EXEMPLE_SOURCE}.cpp")
        target_link_libraries(${SOURCE_NAME} 
            VKInterface 
            glfw
            pthread
            dl
            vulkan
            X11
            Xxf86vm
            Xi
        )
    endforeach()
endif()

