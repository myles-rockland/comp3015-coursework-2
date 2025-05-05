#ifndef SCENEBASIC_UNIFORM_H
#define SCENEBASIC_UNIFORM_H

#include "helper/scene.h"

#include <glad/glad.h>
#include "helper/glslprogram.h"
#include "GLFW/glfw3.h"

// GLM
#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
// Helper files
#include "helper/plane.h"
#include "helper/objmesh.h"
#include "helper/skybox.h"
#include "helper/random.h"
#include "helper/particleutils.h"
#include "Spotlight.h"

class SceneBasic_Uniform : public Scene
{
private:
    GLSLProgram hdrBloomProg, pbrProg, skyboxProg, particlesProg;

    Random rand;
    GLuint initPos, initVel, startTime, particles, nParticles;
    vec3 emitterPos, emitterDir;
    float time, particleLifetime;

    std::unique_ptr<ObjMesh> gun, target;
    Plane plane;
    SkyBox skybox;
    Spotlight spotlight;

    // FBOs, textures and samplers
    GLuint fsQuad, hdrFbo, blurFbo, hdrTex, tex1, tex2;
    GLuint linearSampler, nearestSampler;
    int bloomBufWidth, bloomBufHeight;

    float tPrev;
    float angle;
    float rotSpeed;

    bool whiteLightsEnabled, bloomEnabled;
    bool rightClickedLastFrame;

    // Mouse variables
    glm::vec3 cameraPosition, cameraForward, cameraUp; // Relative position within world space
    float cameraYaw, cameraPitch; // Camera sideways rotation
    float cameraSpeed;
    const float cameraSensitivity; // Mouse sensitivity

    bool mouseFirstEntry; // Determines if first entry of mouse into window

    //Positions of camera from given last frame
    float lastXPos;
    float lastYPos;

    // Default textures
    GLuint defaultAlbedoTexture, defaultNormalTexture, defaultMetallicTexture, defaultRoughnessTexture, defaultAOTexture;

    // Gun textures
    GLuint gunAlbedoTexture, gunNormalTexture, gunMetallicTexture, gunRoughnessTexture, gunAOTexture;

    // Target textures
    GLuint targetAlbedoTexture, targetNormalTexture, targetMetallicTexture, targetRoughnessTexture, targetAOTexture;

    // Particles texture
    GLuint particlesTexture;

    void compile();
    void setupFBO();
    void setupHdrFBO();
    void setupBlurFBO();
    void pass1();
    void pass2();
    void pass3();
    void pass4();
    void pass5();
    void computeLogAveLuminance();
    void drawScene();
    float gauss(float, float);

    // New
    void setSpotlightIntensity(float intensity);
    void setSpotlightInnerCutoff(float degrees);
    void setSpotlightOuterCutoff(float degrees);
    void setupTextures();
    void bindPbrTextures(GLuint albedo, GLuint normal, GLuint metallic, GLuint roughness, GLuint ao);
    void setupFullscreenQuad();
    void computeWeights();
    void setupSamplers();

    void handleKeyboardInput(GLFWwindow* windowContext, float deltaTime);
    void handleMouseMovement(GLFWwindow* windowContext, float deltaTime);
    void handleMouseClicks(GLFWwindow* windowContext);

    void initBuffers();
    float randFloat();
    void setupParticles();

public:
    SceneBasic_Uniform();

    void initScene();
    void update( float t );
    void render();
    void resize(int, int);
    void setMatrices(GLSLProgram& p);
};

#endif // SCENEBASIC_UNIFORM_H