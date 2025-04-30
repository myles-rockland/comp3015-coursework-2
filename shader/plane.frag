#version 460

in vec3 Position;
in vec3 Normal;

layout (location = 0) out vec4 FragColor;

uniform struct LightInfo
{
    vec4 Position;
    vec3 La;
    vec3 L;
} Light;

uniform struct MaterialInfo
{
    vec3 Albedo;
    float Metallic;
    float Roughness;
    float AO;
} Material;

uniform struct SpotLightInfo
{
    vec4 Position;
    vec3 L;
    vec3 Direction;
    float Cutoff;
} Spotlights[3];

const float PI = 3.14159265358979323846;

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
    float metalness = Material.Metallic;
    vec3 albedo = Material.Albedo;
    f0 = mix(f0, albedo.rgb, metalness);

    // Calculate specular component
    float roughness = Material.Roughness; // Transform sample into float representing grey shade. Might need to do 1 - roughness
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

void main() {
    vec3 Colour = vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 3; i++)
    {
        Colour += microfacetModel(i, Position, normalize(Normal));
    }

    vec3 ambient = vec3(0.03) * Material.Albedo * Material.AO;
    Colour += ambient;

    FragColor = vec4(Colour, 1.0);
}
