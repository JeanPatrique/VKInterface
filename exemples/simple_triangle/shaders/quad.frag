#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in  vec3 gradientColor;

void main()
{
    outColor = vec4(gradientColor, 1.0);
}

