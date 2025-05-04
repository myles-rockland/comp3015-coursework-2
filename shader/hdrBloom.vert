#version 460

// In variables
layout (location = 0) in vec3 VertexPosition;
layout (location = 2) in vec2 VertexTexCoord;

// Out variables
out vec2 TexCoord;

uniform mat4 ModelViewMatrix;
uniform mat4 MVP;

void main()
{
   // Set TexCoord and gl_Position for next stage in pipeline (fragment shader)
    TexCoord = VertexTexCoord;
    gl_Position = MVP * vec4(VertexPosition,1.0);
}
