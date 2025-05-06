#version 460

in float Transp; 
in vec2 TexCoord;

layout (location = 0) out vec4 FragColor;

uniform sampler2D ParticleTex; // Preferably on texture unit 8

void main()
{
	FragColor = texture(ParticleTex, TexCoord);
	FragColor.a *= Transp;
	
}