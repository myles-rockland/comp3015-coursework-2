#version 460

// In variables
layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
layout (location = 3) in vec4 VertexTangent;

// Out variables
out vec2 TexCoord;
out vec3 Position;

out vec3 TangentFragPos;
out vec3 TangentCameraPos;
out vec3 TangentSpotlightPos;
out vec3 TangentSpotlightDir;

// Uniforms
uniform struct SpotLightInfo
{
    vec4 Position;
    vec3 Direction;
    vec3 L;
    float InnerCutoff;
    float OuterCutoff;
} Spotlight;

uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;
uniform mat4 MVP;

uniform vec4 CameraPos;

void main()
{
    // Transform normal and tangent to view/camera/eye space
    vec3 normal = normalize(NormalMatrix * VertexNormal);
    vec3 tangent = normalize(NormalMatrix * vec3(VertexTangent));
    vec3 binormal = normalize(cross(normal, tangent));

    // Set matrix for transformation from view space to tangent space
    mat3 TBN = transpose(mat3(tangent, binormal, normal));

    // Get camera position in tangent space
    TangentCameraPos = TBN * CameraPos.xyz;

    // Get fragment position in view space and tangent space
    Position = (ModelViewMatrix * vec4(VertexPosition, 1.0f)).xyz;
    TangentFragPos = TBN * Position;

    // Set spotlight positions in tangent space
    TangentSpotlightPos = TBN * Spotlight.Position.xyz;
    TangentSpotlightDir = TBN * Spotlight.Direction;

    // Set TexCoord and gl_Position for next stage in pipeline (fragment shader)
    TexCoord = VertexTexCoord;
    gl_Position = MVP * vec4(VertexPosition,1.0);
}
