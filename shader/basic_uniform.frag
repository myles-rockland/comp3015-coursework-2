#version 460

in vec3 ViewDir;
in vec2 TexCoord;
in vec3 Position;

layout (location = 0) out vec4 FragColor;

layout (binding = 3) uniform sampler2D AlbedoTexture;
layout (binding = 4) uniform sampler2D NormalTexture;
layout (binding = 5) uniform sampler2D MetalTexture;
layout (binding = 6) uniform sampler2D RoughnessTexture;

layout (binding = 0) uniform sampler2D HdrTex;
layout (binding = 1) uniform sampler2D BlurTex1;
layout (binding = 2) uniform sampler2D BlurTex2;

uniform struct SpotLightInfo
{
    vec4 Position;
    vec3 L;
    vec3 Direction;
    //float InnerCutoff; // InnerCutoff
    float Cutoff; // OuterCutoff
} Spotlights[3];

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

const float PI = 3.14159265358979323846;

float luminance(vec3 colour)
{
    return colour.r * 0.2126 + colour.g * 0.7152 + colour.b * 0.0722;
}

// normal distribution function for approximating ratio of microfacets aligned to h
float ggxDistribution(float nDotH, float roughness) 
{ 
    float alpha2 = roughness * roughness * roughness * roughness;
    float d = (nDotH * nDotH) * (alpha2 - 1) + 1;
    return alpha2 / (PI * d * d);
}

// geometry function for approximating area where microfacets overshadow each other, causing light occlusion
float geomSchlickGGX(float dotProd, float roughness)
{
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float denom = dotProd * (1 - k) + k;
    return 1.0 / denom; // This should be dotProd / denom, but this saves some computation. Tradeoff between visual accuracy and performance
}

vec3 fresnelSchlick(float dotProd, vec3 f0) // Fresnel reflection equation
{
    return f0 + (1 - f0) * pow(1.0 - dotProd, 5);
}

vec3 microfacetModel(int lightIndex, vec3 position, vec3 n) // Reflectance Equation
{ 
    // lights in reflectance
    vec3 l = vec3(0.0); // Direction towards light
    vec3 lightIntensity = vec3(0.0); // AKA Radiance/Strength/Magnitude/Colour

    vec3 s = normalize(Spotlights[lightIndex].Position.xyz - position);

    float cosAngle = dot(-s, normalize(Spotlights[lightIndex].Direction));
    float angle = acos(cosAngle);

    if (angle >= 0.0f && angle < Spotlights[lightIndex].Cutoff) // OuterCutoff
    {
        // let's say 10 and 15
        // we have 11
        // 11 - 10 = 1
        // 15 - 10 = 5
        // 1/5 = 0.2
        //float interpolation = (angle - Spotlights[lightIndex].InnerCutoff) / (Spotlights[lightIndex].OuterCutoff - Spotlights[lightIndex].InnerCutoff);
        //float epsilon = cos(Spotlights[lightIndex].OuterCutoff) - cos(Spotlights[lightIndex].InnerCutoff);
        //float intensity = clamp((cosAngle - cos(Spotlights[lightIndex].OuterCutoff)) / epsilon, 0.0, 1.0);
        lightIntensity = Spotlights[lightIndex].L;

        l = Spotlights[lightIndex].Position.xyz - position;
        float dist = length(l);
        l = normalize(l);
        lightIntensity /= (dist * dist); // attenuation
    }

    // Get metalness and albedo
    vec3 f0 = vec3(0.04);
    float metalness = texture(MetalTexture, TexCoord).r;
    vec3 albedo = pow(texture(AlbedoTexture, TexCoord).rgb, vec3(Gamma)); // convert to linear space as albedo is authored in sRGB space
    f0 = mix(f0, albedo.rgb, metalness);

    // Calculate specular component
    float roughness = max(dot(texture(RoughnessTexture, TexCoord), vec4(1.0)), 0.0); // Transform sample into float representing grey shade. Might need to do 1 - roughness
    vec3 v = normalize(-position);
    vec3 h = normalize(v + l);
    float nDotH = max(dot(n, h), 0.0); // Ensure non-negative values
    float vDotH = max(dot(v, h), 0.0);
    float nDotL = max(dot(n, l), 0.0);
    float nDotV = max(dot(n, v), 0.0);

    // Cook-Torrance BRDF specular part
    float normalDistribution = ggxDistribution(nDotH, roughness);
    float geometry = geomSchlickGGX(nDotL, roughness) * geomSchlickGGX(nDotV, roughness);
    vec3 fresnel = fresnelSchlick(vDotH, f0);
    
    vec3 specularComponent = fresnel;
    vec3 numerator = normalDistribution * geometry * fresnel;
    float denominator = 4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0) + 0.0001; // Add epsilon for non-zero denominator
    vec3 specular = numerator / denominator;

    // diffuse part
    vec3 diffuseComponent = vec3(1.0) - specularComponent;
    diffuseComponent *= 1.0 - metalness;
    vec3 diffuse = diffuseComponent * (albedo / PI);    
    
    // return final radiance
    return (diffuse + specular) * lightIntensity * nDotL;
}

// Pass 1 applies normal mapping, alpha discard and blinnphong lighting for 3 spotlights
vec4 pass1()
{
    // Calculate normal direction from normal map texture
    vec3 norm = texture(NormalTexture, TexCoord).xyz;
    norm.xy = 2.0f * norm.xy - 1.0f; // - 1.0f applies to each component in the vector! https://learnwebgl.brown37.net/12_shader_language/glsl_mathematical_operations.html

    vec3 Colour = vec3(0.0f, 0.0f, 0.0f);

    // Loop through all lights for PBR
    if (gl_FrontFacing)
    {
        for (int i = 0; i < 3; i++)
        {
            //Colour += blinnphongSpot(normalize(norm), i);
            Colour += microfacetModel(i, Position, normalize(norm));
        }
    }
    else
    {
        for (int i = 0; i < 3; i++)
        {
            //Colour += blinnphongSpot(normalize(-norm), i);
            Colour += microfacetModel(i, Position, normalize(-norm));
        }
    }

    return vec4(Colour, 1);
}

// Pass 2 is checking hdr texture sample against luminance threshold
vec4 pass2()
{
    vec4 val = texture(HdrTex, TexCoord);
    
    if(luminance(val.rgb) > LumThresh)
    {
        return val;
    }
    else
    {
        return vec4(0.0);
    }
}

// Pass 3 is applying blur in y direction
vec4 pass3()
{
    float dy = 1.0 / (textureSize(BlurTex1, 0)).y;
    vec4 sum = texture(BlurTex1, TexCoord) * Weight[0];
    for(int i = 0; i < 10; i++)
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
    for(int i = 0; i < 10; i++)
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
    float xyzSum = xyzCol.x + xyzCol.y + xyzCol.z;
    vec3 xyYCol = vec3(xyzCol.x / xyzSum, xyzCol.y / xyzSum, xyzCol.y);

    // Apply the tone mapping operation to the luminance (xyYCol.z or xyzCol.y)
    float L = (Exposure * xyYCol.z) / 0.5; //float L = (Exposure * xyYCol.z) / AveLum;
    L = (L * (1 + L / (White * White))) / (1 + L);

    // Using the new luminance, convert back to XYZ
    xyzCol.x = (L * xyYCol.x) / (xyYCol.y);
    xyzCol.y = L;
    xyzCol.z = (L * (1 - xyYCol.x - xyYCol.y))/xyYCol.y;

    // Convert back to RGB
    vec4 toneMapColor = vec4(xyz2rgb * xyzCol, 1.0);

    // Gamma correct
    toneMapColor = vec4(pow(vec3(toneMapColor), vec3(1.0 / Gamma)), 1.0);

    return toneMapColor;
}

void main() {
    if (Pass == 1) FragColor = pass1();
    else if (Pass == 2) FragColor = pass2();
    else if (Pass == 3) FragColor = pass3();
    else if (Pass == 4) FragColor = pass4();
    else if (Pass == 5) FragColor = pass5();
}
