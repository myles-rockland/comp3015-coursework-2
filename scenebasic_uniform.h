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
#include "helper/torus.h"
#include "helper/teapot.h"
#include "helper/plane.h"
#include "helper/objmesh.h"
#include "helper/cube.h"
#include "helper/skybox.h"

class SceneBasic_Uniform : public Scene
{
private:
    GLSLProgram hdrBloomProg, pbrProg, skyboxProg;
    glm::mat4 rotationMatrix;

    std::unique_ptr<ObjMesh> gun;
    Plane plane;
    SkyBox skybox;

    GLuint fsQuad, hdrFbo, blurFbo, hdrTex, tex1, tex2;
    GLuint linearSampler, nearestSampler;
    int bloomBufWidth, bloomBufHeight;

    float tPrev;
    float angle;
    float rotSpeed;

    bool whiteLightsEnabled;
    bool bloomEnabled;

    // Mouse variables
    glm::vec3 cameraPosition; // Relative position within world space
    glm::vec3 cameraForward; // Forwards direction of travel
    glm::vec3 cameraUp; // Up direction within world space
    float cameraYaw; // Camera sideways rotation
    float cameraPitch; // Camera vertical rotation
    float cameraSpeed;
    const float cameraSensitivity; // Mouse sensitivity

    bool mouseFirstEntry; // Determines if first entry of mouse into window

    //Positions of camera from given last frame
    float lastXPos;
    float lastYPos;

    // Gun textures
    GLuint gunAlbedoTexture;
    GLuint gunNormalTexture;
    GLuint gunMetallicTexture;
    GLuint gunRoughnessTexture;
    GLuint gunAOTexture;

    // Default textures
    GLuint defaultAlbedoTexture;
    GLuint defaultNormalTexture;
    GLuint defaultMetallicTexture;
    GLuint defaultRoughnessTexture;
    GLuint defaultAOTexture;

    void compile();
    void setupFBO();
    void pass1();
    void pass2();
    void pass3();
    void pass4();
    void pass5();
    void computeLogAveLuminance();
    void drawScene();
    float gauss(float, float);

    // New
    void setSpotlightsIntensity(float intensity);
    void setSpotlightsInnerCutoff(float degrees);
    void setSpotlightsOuterCutoff(float degrees);
    void setupTextures();
    void bindPbrTextures(GLuint albedo, GLuint normal, GLuint metallic, GLuint roughness, GLuint ao);
    void setupFullscreenQuad();
    void computeWeights();
    void setupSamplers();

    void handleKeyboardInput(GLFWwindow* windowContext, float deltaTime);
    void handleMouseMovement(GLFWwindow* windowContext, float deltaTime);
    void handleMouseClicks(GLFWwindow* windowContext);

public:
    SceneBasic_Uniform();

    void initScene();
    void update( float t );
    void render();
    void resize(int, int);
    void setMatrices(GLSLProgram& p);
};

#endif // SCENEBASIC_UNIFORM_H
