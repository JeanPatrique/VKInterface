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

# Options :
option(VKI_ENABLE_DEBUG_LOGS            "Enable some extra log calls"   OFF)
option(VKI_BUILD_EXEMPLE_EXECUTABLES    "Build exemple executables."    OFF)

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

# Options :
if (${VKI_ENABLE_DEBUG_LOGS})
    target_compile_definitions(VKInterface PUBLIC VKI_ENABLE_DEBUG_LOGS)
    message("-- Adding VKI_ENABLE_DEBUG_LOGS macro.")
endif()

# Building exemples :
if (${VKI_BUILD_EXEMPLE_EXECUTABLES})

    message("-- Building exemples.")
    project("Vki_demo")

    # Create a function to compile a glsl shader to a spir-v bytecode.
    function(func_CompileShader input output)
        add_custom_command(
            OUTPUT "${output}"
            COMMAND glslc ARGS "${input}" -o "${output}" 
            MAIN_DEPENDENCY "${input}"
            VERBATIM
        )
    endfunction()

    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/exemples")
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/exemples/simple_triangle") # The first triangle is precious <3
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/exemples/compute_texture")
    #add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/exemples/dummyEngine") # It took so long :/, I'd rather make a true one straight.
    #add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/exemples/tests") # TODO uncomment this !
    
    # 
    #target_link_libraries(${source_name} 
    #    vkinterface 
    #    glfw
    #    pthread
    #    dl
    #    vulkan
    #    x11
    #    xxf86vm
    #    xi
    #)
endif()

