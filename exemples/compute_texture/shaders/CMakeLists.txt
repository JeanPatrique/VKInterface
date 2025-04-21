cmake_minimum_required(VERSION 3.16)

message("-- Configure shaders compilation")

set(SHADER_SOURCES
	"quad.vert"
	"quad.frag"
	)

set(SHADER_COMPILED)

foreach( SHADER_SOURCE ${SHADER_SOURCES})
    set(SHADER_DEST "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_SOURCE}.spv")

    func_CompileShader(
        "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER_SOURCE}"      # input
        "${SHADER_DEST}"  # output
    )

    list(APPEND SHADER_COMPILED "${SHADER_DEST}")

endforeach()

add_custom_target(computeTexture_compile_shaders ALL
    DEPENDS ${SHADER_COMPILED}
)

#add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/quad.frag
#    COMMAND glslc ARGS ${CMAKE_CURRENT_DIR}/quad.frag -o ${CMAKE_BUILD_DIR}/quad.frag.spv
#    MAIN_DEPENDENCY ${CMAKE_CURRENT_DIR}/quad.frag
#    VERBATIM
#)
