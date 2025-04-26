#version 460

// In variables
layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
layout (location = 3) in vec4 VertexTangent;

// Out variables
out vec3 ViewDir;
out vec2 TexCoord;
out vec3 Position;

uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;
uniform mat4 MVP;

void main()
{
    // Transform normal and tangent to eye space
    vec3 normal = normalize(NormalMatrix * VertexNormal);
    vec3 tangent = normalize(NormalMatrix * vec3(VertexTangent));
    vec3 binormal = normalize(cross(normal, tangent)) * VertexTangent.w; // Compute binormal
    Position = (ModelViewMatrix * vec4(VertexPosition, 1.0f)).xyz;

    // Set matrix for transformation to tangent space
    mat3 toObjectLocal = mat3(
        tangent.x, binormal.x, normal.x,
        tangent.y, binormal.y, normal.y,
        tangent.z, binormal.z, normal.z
    );

    // Get light direction and view direction in tangent space
    ViewDir = toObjectLocal * normalize(-Position);

    // Set TexCoord and gl_Position for next stage in pipeline (fragment shader)
    TexCoord = VertexTexCoord;
    gl_Position = MVP * vec4(VertexPosition,1.0);
}
