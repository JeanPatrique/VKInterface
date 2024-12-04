
add_executable(simpleTriangle "${CMAKE_CURRENT_SOURCE_DIR}/simple_triangle.cpp")

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
