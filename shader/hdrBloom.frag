#version 460

in vec2 TexCoord;

layout (location = 0) out vec4 FragColor;

layout (binding = 0) uniform sampler2D HdrTex;
layout (binding = 1) uniform sampler2D BlurTex1;
layout (binding = 2) uniform sampler2D BlurTex2;

uniform int Pass;
uniform float LumThresh;
uniform float PixOffset[10] = float[](0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);
uniform float Weight[10];

uniform float AveLum;
uniform float Exposure;
uniform float White;
uniform float Gamma;

uniform bool BloomEnabled;

uniform mat3 rgb2xyz = mat3(
    0.4142564, 0.2126729, 0.0193339,
    0.3575761, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
);

uniform mat3 xyz2rgb = mat3(
    3.2404542, -0.9692660, 0.0556434,
    -1.5371385, 1.8760108, -0.2040259,
    -0.4985314, 0.0415560, 1.0572252
);

float luminance(vec3 colour)
{
    return colour.r * 0.2126 + colour.g * 0.7152 + colour.b * 0.0722;
}

// Pass 2 is checking hdr texture sample against luminance threshold
vec4 pass2()
{
    vec4 val = texture(HdrTex, TexCoord);
    return (luminance(val.rgb) > LumThresh) ? val : vec4(0.0);
}

// Pass 3 is applying blur in y direction
vec4 pass3()
{
    float dy = 1.0 / (textureSize(BlurTex1, 0)).y;
    vec4 sum = texture(BlurTex1, TexCoord) * Weight[0];
    for(int i = 1; i < 10; i++)
    {
        sum += texture(BlurTex1, TexCoord + vec2(0.0, PixOffset[i]) * dy) * Weight[i];
        sum += texture(BlurTex1, TexCoord - vec2(0.0, PixOffset[i]) * dy) * Weight[i];
    }
    return sum;
}

// Pass 4 is applying blur in x direction
vec4 pass4()
{
    float dx = 1.0 / (textureSize(BlurTex2, 0)).x;
    vec4 sum = texture(BlurTex2, TexCoord) * Weight[0];
    for(int i = 1; i < 10; i++)
    {
        sum += texture(BlurTex2, TexCoord + vec2(PixOffset[i], 0.0) * dx) * Weight[i];
        sum += texture(BlurTex2, TexCoord - vec2(PixOffset[i], 0.0) * dx) * Weight[i];
    }
    return sum;
}

// Bloom + HDR Tone Mapping + Gamma Correction
vec4 pass5() 
{
    // Retrieve high-res color from texture
    vec4 color = texture(HdrTex, TexCoord);

    if (BloomEnabled)
    {
        // Combine with blurred texture for Bloom
        // We want linear filtering on this texture access so that we get additional blurring.
        vec4 blurTex = texture(BlurTex1, TexCoord);
        color += blurTex;
    }

    // Convert from RGB to CIE XYZ
    vec3 xyzCol = rgb2xyz * vec3(color);

    // Convert from XYZ to xyY
    float xyzSum = max(xyzCol.x + xyzCol.y + xyzCol.z, 0.00001);
    vec3 xyYCol = vec3(xyzCol.x / xyzSum, xyzCol.y / xyzSum, xyzCol.y);

    // Apply the tone mapping operation to the luminance (xyYCol.z or xyzCol.y)
    float L = (Exposure * xyYCol.z) / AveLum;
    L = (L * (1 + L / (White * White))) / (1 + L);

    // Using the new luminance, convert back to XYZ
    xyzCol.x = (L * xyYCol.x) / max(xyYCol.y, 0.00001);
    xyzCol.y = L;
    xyzCol.z = (L * (1 - xyYCol.x - xyYCol.y))/ max(xyYCol.y, 0.00001);

    // Convert back to RGB
    vec4 toneMapColor = vec4(xyz2rgb * xyzCol, 1.0);

    // Gamma correct
    toneMapColor = vec4(pow(vec3(toneMapColor), vec3(1.0 / Gamma)), 1.0);

    return toneMapColor;
}

void main() {
    if (Pass == 2) FragColor = pass2();
    else if (Pass == 3) FragColor = pass3();
    else if (Pass == 4) FragColor = pass4();
    else if (Pass == 5) FragColor = pass5();
}
