#version 460

in vec3 ViewDir;
in vec2 TexCoord;
in vec3 Position;

layout (location = 0) out vec4 FragColor;

layout (binding = 3) uniform sampler2D DiffuseTexture;
layout (binding = 4) uniform sampler2D NormalTexture;
layout (binding = 5) uniform sampler2D AlphaTexture;

layout (binding = 0) uniform sampler2D HdrTex;
layout (binding = 1) uniform sampler2D BlurTex1;
layout (binding = 2) uniform sampler2D BlurTex2;

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

uniform int Pass;
uniform float LumThresh;
uniform float PixOffset[10] = float[](0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);
uniform float Weight[10];

uniform float AveLum;
uniform float Exposure;
uniform float White;
uniform float Gamma;

uniform bool AlphaMapEnabled;
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

vec3 blinnphongSpot(vec3 n, int lightIndex)
{
    vec3 ambient = vec3(0), diffuse = vec3(0), specular = vec3(0);

    // Get base texture colour from diffuse texture
    vec3 texColour = texture(DiffuseTexture, TexCoord).rgb;

    // Ambient
    ambient = Spotlights[lightIndex].La * texColour;

    // Diffuse
    vec3 s = normalize(Spotlights[lightIndex].Position.xyz - Position);

    float cosAngle = dot(-s, normalize(Spotlights[lightIndex].Direction));
    float angle = acos(cosAngle);
    float spotScale;

    if (angle >= 0.0f && angle < Spotlights[lightIndex].Cutoff)
    {
        spotScale = pow(cosAngle, Spotlights[lightIndex].Exponent);
        float sDotN = max(dot(s, n), 0.0);
        diffuse = texColour * sDotN;

        // Specular
        if (sDotN > 0.0)
        {
            vec3 v = normalize(ViewDir);
            vec3 h = normalize(v + s);
            specular = Material.Ks * pow(max(dot(h,n),0.0), Material.Shininess);
        }
    }

    return ambient + spotScale * Spotlights[lightIndex].L * (diffuse + specular);
}

// Pass 1 applies normal mapping, alpha discard and blinnphong lighting for 3 spotlights
vec4 pass1()
{
    // Calculate normal direction from normal map texture
    vec3 norm = texture(NormalTexture, TexCoord).xyz;
    norm.xy = 2.0f * norm.xy - 1.0f; // - 1.0f applies to each component in the vector! https://learnwebgl.brown37.net/12_shader_language/glsl_mathematical_operations.html

    vec3 Colour = vec3(0.0f, 0.0f, 0.0f);

    // Discard using alpha map
    vec4 alphaMap = texture(AlphaTexture, TexCoord);

    if (AlphaMapEnabled && alphaMap.a < 0.15f)
    {
        discard;
    }
    else
    {
        if (gl_FrontFacing)
        {
            for (int i = 0; i < 3; i++)
            {
                Colour += blinnphongSpot(normalize(norm), i);
            }
        }
        else
        {
            for (int i = 0; i < 3; i++)
            {
                Colour += blinnphongSpot(normalize(-norm), i);
            }
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

vec4 pass5() 
{
    // HDR + Tone Mapping
    // Retrieve high-res color from texture
    vec4 color = texture(HdrTex, TexCoord);

    // Convert from RGB to CIE XYZ
    vec3 xyzCol = rgb2xyz * vec3(color);

    // Convert from XYZ to xyY
    float xyzSum = xyzCol.x + xyzCol.y + xyzCol.z;
    vec3 xyYCol = vec3( xyzCol.x / xyzSum, xyzCol.y / xyzSum, xyzCol.y);

    // Apply the tone mapping operation to the luminance (xyYCol.z or xyzCol.y)
    float L = (Exposure * xyYCol.z) / AveLum;
    L = (L * ( 1 + L / (White * White) )) / ( 1 + L );

    // Using the new luminance, convert back to XYZ
    xyzCol.x = (L * xyYCol.x) / (xyYCol.y);
    xyzCol.y = L;
    xyzCol.z = (L * (1 - xyYCol.x - xyYCol.y))/xyYCol.y;

    // Convert back to RGB
    vec4 toneMapColor = vec4( xyz2rgb * xyzCol, 1.0);

    // Combine with blurred texture for Bloom
    // We want linear filtering on this texture access so that we get additional blurring.
    vec4 blurTex = texture(BlurTex1, TexCoord);

    if (BloomEnabled)
        return toneMapColor + blurTex;
    else
        return toneMapColor;
}

void main() {
    if (Pass == 1) FragColor = pass1();
    else if (Pass == 2) FragColor = pass2();
    else if (Pass == 3) FragColor = pass3();
    else if (Pass == 4) FragColor = pass4();
    else if (Pass == 5) FragColor = vec4(pow(vec3(pass5()), vec3(1.0 / Gamma)), 1.0);
}
