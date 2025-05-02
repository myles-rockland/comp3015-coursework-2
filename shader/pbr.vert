#version 460

// In variables
layout (location = 0) in vec3 VertexPosition;
layout (location = 1) in vec3 VertexNormal;
layout (location = 2) in vec2 VertexTexCoord;
layout (location = 3) in vec4 VertexTangent;

// Out variables
out vec2 TexCoord;

out vec3 TangentFragPos;
out vec3 TangentCameraPos;
out vec3 TangentSpotlightsPos[3];
out vec3 TangentSpotlightsDir[3];

// Uniforms
uniform struct SpotLightInfo
{
    vec4 Position;
    vec3 L;
    vec3 Direction;
    float InnerCutoff;
    float OuterCutoff;
} Spotlights[3];

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

    // Get fragment position in tangent space
    TangentFragPos = TBN * (ModelViewMatrix * vec4(VertexPosition, 1.0f)).xyz;

    // Set spotlight positions in tangent space
    for(int i = 0; i < 3; i++)
    {
        TangentSpotlightsPos[i] = TBN * Spotlights[i].Position.xyz;
        TangentSpotlightsDir[i] = TBN * Spotlights[i].Direction;
    }

    // Set TexCoord and gl_Position for next stage in pipeline (fragment shader)
    TexCoord = VertexTexCoord;
    gl_Position = MVP * vec4(VertexPosition,1.0);
}
