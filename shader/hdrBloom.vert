#version 460

// In variables
layout (location = 0) in vec3 VertexPosition;
//layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
//layout (location = 3) in vec4 VertexTangent;

// Out variables
out vec2 TexCoord;
out vec3 Position;

uniform mat4 ModelViewMatrix;
uniform mat4 MVP;

void main()
{
    // Set vertex position to be interpolated in fragment shader for fragment position
    Position = (ModelViewMatrix * vec4(VertexPosition, 1.0f)).xyz;

    // Set TexCoord and gl_Position for next stage in pipeline (fragment shader)
    TexCoord = VertexTexCoord;
    gl_Position = MVP * vec4(VertexPosition,1.0);
}
