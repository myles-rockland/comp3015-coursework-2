# COMP3015 Coursework 2
An OpenGL project showcasing advanced shader techniques including Bloom, PBR, and Particles.

![OpenGL Scene](./images/scene.gif)

This project has been written and tested using Visual Studio 2022 on a Windows 10 machine.

> [!IMPORTANT]
> This project assumes an OpenGL directory has been set up in the "C:/Users/Public/" directory. Please see the project properties in Visual Studio for more details on include directories.

## Attributions
- Pistol with Engravings Model: https://sketchfab.com/3d-models/pistol-with-engravings-bccd75a0016d49448243135a56facb96#download
- Target Model: https://sketchfab.com/3d-models/pbr-target-ea1bec8a10054369862412c6d451e558

## YouTube Video Walkthrough
YouTube Video: Coming soon

## Controls
- Move Around - WASD
- Look Around - Move Mouse
- Toggle Ultraviolet Light - Right Click
- Toggle Bloom - 3

## Feature 1 - PBR
All objects in the scene are rendered in pass 1 with PBR textures (albedo, normal, roughness, metallic, AO maps). Mainly implemented in [pbr.frag](./shader/pbr.frag), and adapted for a flashlight.
```glsl
...
// Pass 1 applies normal mapping, PBR for a flashlight, and fog colouring
vec4 pass1()
{
    // Calculate normal direction from normal map texture. Already in tangent space, so no conversion
    vec3 norm = texture(NormalTexture, TexCoord).xyz;
    norm.xy = 2.0f * norm.xy - 1.0f;
    norm = (gl_FrontFacing) ? normalize(norm) : normalize(-norm);

    // Calculate PBR colour
    vec3 Colour = vec3(0.0f, 0.0f, 0.0f);
    Colour += microfacetModel(TangentFragPos, norm);
    vec3 ambient = vec3(0.03) * texture(AlbedoTexture, TexCoord).rgb * texture(AOTexture, TexCoord).rgb;
    Colour += ambient;

    ...

    return vec4(Colour, 1);
}

void main() {
    FragColor = pass1();
}
```

## Feature 2 - Particle Rain
Rain particles fall from the area above the plane. Setup seen in [scenebasic_uniform.cpp](./scenebasic_uniform.cpp) in `SceneBasic_Uniform::setupParticles()` and `SceneBasic_Uniform::initBuffers()`. Example of setting initial particle positions:
```cpp
void SceneBasic_Uniform::initBuffers()
{
	...
	// Fill the initial position buffer
	float halfX = 100.0f * 0.5f;
	float halfZ = 100.0f * 0.5f;

	for (int i = 0; i < nParticles; i++)
	{
		float randX = glm::mix(-halfX, +halfX, randFloat());
		float randZ = glm::mix(-halfZ, +halfZ, randFloat());
		
		data[3 * i] = randX;
		data[3 * i + 1] = 50.0f;
		data[3 * i + 2] = randZ;
	}

	glBindBuffer(GL_ARRAY_BUFFER, initPos);
	glBufferSubData(GL_ARRAY_BUFFER, 0, size * 3, data.data());
	...
}
```
Example code in [particles.vert](./shader/particles.vert) that calculates particle positions based on SUVAT equations:
```glsl
...
// Offsets to the position in camera coordinates for each vertex of the particle's quad
const vec3 offsets[] = vec3[](vec3(-0.5,-0.5,0), vec3(0.5,-0.5,0), vec3(0.5,0.5,0),
							  vec3(-0.5,-0.5,0), vec3(0.5,0.5,0), vec3(-0.5,0.5,0));
// Texture coordinates for each vertex of the particle's quad
const vec2 texCoords[] = vec2[](vec2(0,0), vec2(1,0), vec2(1,1), vec2(0,0), vec2(1,1), vec2(0,1));

void main()
{
	vec3 cameraPos; // Position in camera coordinates

	float t = Time - VertexBirthTime;
	if( t >= 0 && t < ParticleLifetime) {
		vec3 pos = VertexInitPos + VertexInitVel * t + Gravity * t * t;
		cameraPos = (ModelViewMatrix * vec4(pos,1)).xyz + (offsets[gl_VertexID] * ParticleSize);  //offset the vertex based on the ID
		Transp = mix (1, 0, t / ParticleLifetime);
	} else {
		// Particle doesn't "exist", draw fully transparent
		cameraPos = vec3(0);
		Transp = 0.0;
	}
	TexCoord = texCoords[gl_VertexID];
	gl_Position = ProjectionMatrix * vec4(cameraPos, 1);
}
```

## Feature 3 - Bloom
Bloom effect is applied in render passes 2-5 in [hdrBloom.frag](./shader/hdrBloom.frag):
```glsl
...
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
	...
```

## Feature 4 - Fog
Black fog effect is applied to objects rendered with PBR in pass 1 of [pbr.frag](./shader/pbr.frag):
```glsl
vec4 pass1()
{
    ...
	// Calculate Fog colour
	float dist = length(Position - CameraPos.xyz);
	float fogFactor = (Fog.MaxDist - dist)/(Fog.MaxDist - Fog.MinDist);
	fogFactor = clamp(fogFactor, 0.0, 1.0);
	Colour = mix(Fog.Colour, Colour, fogFactor);

	return vec4(Colour, 1);
}
```