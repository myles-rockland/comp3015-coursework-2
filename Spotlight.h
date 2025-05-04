#pragma once
#include "helper/scene.h"

#include <glad/glad.h>
#include "helper/glslprogram.h"
#include "GLFW/glfw3.h"

// GLM
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace glm;

class Spotlight
{
private:
    vec4 position;
    vec3 direction;
    vec3 L; // Light intensity
    float innerCutoff, outerCutoff; // Stored in radians
public:
    Spotlight(vec4 pos, vec3 dir, vec3 intensity, float inner, float outer);
    void setPosition(vec4 pos);
    vec4 getPosition();
    void setDirection(vec3 dir);
    vec3 getDirection();
    void setIntensity(vec3 intensity);
    vec3 getIntensity();
    void setInnerCutoff(float degrees);
    float getInnerCutoff();
    void setOuterCutoff(float degrees);
    float getOuterCutoff();
    mat4 getViewMatrix();
    mat4 getProjectionMatrix();
};