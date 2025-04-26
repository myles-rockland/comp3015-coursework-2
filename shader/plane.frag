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
    vec3 Kd;
    vec3 Ka;
    vec3 Ks;
    float Shininess;
} Material;

uniform struct SpotLightInfo
{
    vec4 Position;
    vec3 La;
    vec3 L;
    vec3 Direction;
    float Exponent;
    float Cutoff;
} Spotlights[3];

vec3 blinnphong(vec3 position, vec3 n)
{
    vec3 ambient = vec3(0.0f), diffuse = vec3(0.0f), specular = vec3(0.0f);

    // Ambient
    ambient = Light.La * Material.Ka;

    // Diffuse
    vec3 s = normalize(Light.Position.xyz - position);
    float sDotN = max(dot(s, n), 0.0);
    diffuse = Material.Kd * sDotN;

    // Specular
    if (sDotN > 0.0)
    {
        vec3 v = normalize(-position.xyz);
        vec3 h = normalize(v + s);
        specular = Material.Ks * pow(max(dot(h,n),0.0), Material.Shininess);
    }

    return ambient + (diffuse + specular) * Light.L;
}

vec3 blinnphongSpot(vec3 position, vec3 n, int lightIndex)
{
    vec3 ambient = vec3(0), diffuse = vec3(0), specular = vec3(0);

    // Ambient
    ambient = Spotlights[lightIndex].La * Material.Ka;

    // Diffuse
    vec3 s = normalize(Spotlights[lightIndex].Position.xyz - position);

    float cosAngle = dot(-s, normalize(Spotlights[lightIndex].Direction));
    float angle = acos(cosAngle);
    float spotScale;

    if (angle >= 0.0f && angle < Spotlights[lightIndex].Cutoff)
    {
        spotScale = pow(cosAngle, Spotlights[lightIndex].Exponent);
        float sDotN = max(dot(s, n), 0.0);
        diffuse = Material.Kd * sDotN;

        // Specular
        if (sDotN > 0.0)
        {
            vec3 v = normalize(-position.xyz);
            vec3 h = normalize(v + s);
            specular = Material.Ks * pow(max(dot(h,n),0.0), Material.Shininess);
        }
    }

    return ambient + spotScale * Spotlights[lightIndex].L * (diffuse + specular);
}

void main() {
    vec3 Colour = vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 3; i++)
    {
        Colour += blinnphongSpot(Position, normalize(Normal), i);
    }
    FragColor = vec4(Colour, 1.0);
    
    //FragColor = vec4(blinnphong(Position, normalize(Normal)), 1.0);
}
