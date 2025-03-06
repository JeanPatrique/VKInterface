#version 450
layout(location = 0) in vec2  vertexPosition;
layout(location = 1) in vec3  vertexColor;
layout(location = 0) out vec3 gradientColor;

//uniform vec2 screenSize;

/*
const vec2 meshPoints[3] = vec2[](
            vec2( 0.0, -0.5),
            vec2( 0.5,  0.5),
            vec2(-0.5,  0.5)
        );
*/
const vec3 gradient[3] = vec3[](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0)
        );

void main()
{
    gl_Position = vec4(vertexPosition, 0.0, 1.0);

    gradientColor = vertexColor;
}

