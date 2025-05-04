#version 460

in vec2 TexCoord;

in vec3 TangentFragPos;
in vec3 TangentCameraPos;
in vec3 TangentSpotlightPos;
in vec3 TangentSpotlightDir;

layout (location = 0) out vec4 FragColor;

layout (binding = 3) uniform sampler2D AlbedoTexture;
layout (binding = 4) uniform sampler2D NormalTexture;
layout (binding = 5) uniform sampler2D MetalTexture;
layout (binding = 6) uniform sampler2D RoughnessTexture;
layout (binding = 7) uniform sampler2D AOTexture;

layout (binding = 0) uniform sampler2D HdrTex;
//layout (binding = 1) uniform sampler2D BlurTex1;
//layout (binding = 2) uniform sampler2D BlurTex2;

uniform struct SpotLightInfo
{
    vec4 Position;
    vec3 Direction;
    vec3 L;
    float InnerCutoff;
    float OuterCutoff;
} Spotlight;

uniform float Gamma;

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

vec3 fresnelSchlick(float dotProd, vec3 f0) // Fresnel reflection equation for specular component
{
    return f0 + (1 - f0) * pow(1.0 - dotProd, 5);
}

vec3 microfacetModel(vec3 position, vec3 n) // Reflectance Equation
{ 
    // lights in reflectance
    vec3 l = vec3(0.0); // Direction towards light
    vec3 lightIntensity = vec3(0.0); // AKA Radiance/Strength/Magnitude/Colour

    vec3 s = normalize(TangentSpotlightPos - position);

    float cosAngle = dot(-s, normalize(TangentSpotlightDir));
    float angle = acos(cosAngle);

    if (angle >= 0.0f && angle < Spotlight.OuterCutoff) // OuterCutoff
    {
        //float interpolation = (angle - Spotlights[lightIndex].InnerCutoff) / (Spotlights[lightIndex].OuterCutoff - Spotlights[lightIndex].InnerCutoff);
        //float epsilon = cos(Spotlights[lightIndex].OuterCutoff) - cos(Spotlights[lightIndex].InnerCutoff);
        //float intensity = clamp((cosAngle - cos(Spotlights[lightIndex].OuterCutoff)) / epsilon, 0.0, 1.0);
        lightIntensity = Spotlight.L; //* intensity;

        l = TangentSpotlightPos - position;
        float dist = length(l);
        l = normalize(l);
        lightIntensity /= (dist * dist); // attenuation
    }

    // Get metalness and albedo for f0
    vec3 f0 = vec3(0.04);
    float metalness = texture(MetalTexture, TexCoord).r;
    vec3 albedo = pow(texture(AlbedoTexture, TexCoord).rgb, vec3(Gamma)); // convert to linear space as albedo is authored in sRGB space
    f0 = mix(f0, albedo.rgb, metalness);

    // Get values for BRDF
    float roughness = texture(RoughnessTexture, TexCoord).r; // RGB values are the same as they are greyscale, so only one value needed
    vec3 v = normalize(TangentCameraPos - position);
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
    float denominator = 4.0 * nDotV * nDotL + 0.0001; // Add epsilon for non-zero denominator
    vec3 specular = numerator / denominator;

    // diffuse part
    vec3 diffuseComponent = vec3(1.0) - specularComponent;
    diffuseComponent *= 1.0 - metalness;
    vec3 diffuse = diffuseComponent * (albedo / PI); // Lambertian diffuse
    
    // return final radiance
    return (diffuse + specular) * lightIntensity * nDotL;
}

// Pass 1 applies normal mapping, alpha discard and blinnphong lighting for 3 spotlights
vec4 pass1()
{
    // Calculate normal direction from normal map texture. Already in tangent space, so no conversion
    vec3 norm = texture(NormalTexture, TexCoord).xyz;
    norm.xy = 2.0f * norm.xy - 1.0f;
    norm = (gl_FrontFacing) ? normalize(norm) : normalize(-norm);

    vec3 Colour = vec3(0.0f, 0.0f, 0.0f);

    Colour += microfacetModel(TangentFragPos, norm);

    vec3 ambient = vec3(0.03) * texture(AlbedoTexture, TexCoord).rgb * texture(AOTexture, TexCoord).rgb;
    Colour += ambient;

    return vec4(Colour, 1);
}

void main() {
    FragColor = pass1();
}
