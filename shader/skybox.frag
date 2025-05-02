#version 460

in vec3 Vec;
layout (location = 0) out vec4 FragColor;
layout (binding = 0) uniform samplerCube SkyBoxTex;

void main() {
    vec3 texColour = texture(SkyBoxTex, normalize(Vec)).rgb;
    //texColour = pow(texColour, vec3(2.2f)); // sRGB -> linear conversion
    FragColor = vec4(texColour, 1.0);
}