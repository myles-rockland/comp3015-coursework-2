#include "Spotlight.h"

Spotlight::Spotlight(vec4 pos, vec3 dir, vec3 intensity, float innerDegrees, float outerDegrees) : position(vec4(0.0f)), direction(vec3(0.0f, 0.0f, 1.0f)), L(1.0f), innerCutoff(0.98f), outerCutoff(0.96f)
{
	position = pos;
	direction = normalize(dir);
	L = intensity;
	innerCutoff = radians(innerDegrees);
	outerCutoff = radians(outerDegrees);
}

void Spotlight::setPosition(vec4 pos) { position = pos; }

vec4 Spotlight::getPosition() { return position; }

void Spotlight::setDirection(vec3 dir) { direction = normalize(dir); }

vec3 Spotlight::getDirection() { return direction; }

void Spotlight::setIntensity(vec3 intensity) { L = intensity; }

vec3 Spotlight::getIntensity() { return L; }

void Spotlight::setInnerCutoff(float degrees) { innerCutoff = radians(degrees); }

float Spotlight::getInnerCutoff() { return innerCutoff; }

void Spotlight::setOuterCutoff(float degrees) { outerCutoff = radians(degrees); }

float Spotlight::getOuterCutoff() { return outerCutoff; }

mat4 Spotlight::getViewMatrix() { return lookAt(vec3(position), vec3(position) + direction, vec3(0.0f, 1.0f, 0.0f)); }

//mat4 Spotlight::getProjectionMatrix() { return ortho(-10.0f, 10.0f, -10.0f, 10.0f, 1.0f, 25.0f); } // Maybe this could/should be perspective instead of orthographic

mat4 Spotlight::getProjectionMatrix() { return perspective(50.0f, 1.0f, 1.0f, 25.0f); } // Maybe this could/should be perspective instead of orthographic
