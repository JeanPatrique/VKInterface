
# Compile shaders from glsl to spir-v bytecode.
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/shaders")

add_executable(simpleTriangle "${CMAKE_CURRENT_SOURCE_DIR}/simple_triangle.cpp")

#file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/shaders" DESTINATION "${CMAKE_BINARY_DIR}/shaders")

target_link_libraries(simpleTriangle
    VKInterface 
    glfw
    pthread
    dl
    vulkan
    X11
    Xxf86vm
    Xi
)
