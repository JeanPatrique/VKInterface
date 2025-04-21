
# Compile shaders from glsl to spir-v bytecode.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/shaders")

add_executable(compute_texture "${CMAKE_CURRENT_SOURCE_DIR}/compute_texture.cpp")

#file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_BINARY_DIR}/shaders")

target_link_libraries(compute_texture
    VKInterface 
    glfw
    pthread
    dl
    vulkan
    X11
    Xxf86vm
    Xi
)
