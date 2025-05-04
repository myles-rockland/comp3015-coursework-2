#include "scenebasic_uniform.h"

#include <cstdio>
#include <cstdlib>

#include <string>
using std::string;

#include <sstream>
#include <iostream>
using std::cerr;
using std::endl;

#include "helper/glutils.h"
#include "helper/texture.h"

#include "glad/glad.h"

using namespace glm;

SceneBasic_Uniform::SceneBasic_Uniform() :
    plane(100.0f, 100.0f, 1, 1),
    tPrev(0), angle(0.0f), rotSpeed(pi<float>() / 8.0f),
    whiteLightsEnabled(true), bloomEnabled(true),
    cameraPosition(0.0f, 0.0f, 10.0f), cameraForward(0.0f, 0.0f, 1.0f), cameraUp(0.0f, 1.0f, 0.0f),
    cameraYaw(-90.0f), cameraPitch(0.0f),
    cameraSpeed(5.0f), cameraSensitivity(0.025f),
    mouseFirstEntry(true), lastXPos(width / 2.0f), lastYPos(height / 2.0f),
    spotlight(vec4(cameraPosition, 1.0), cameraForward, vec3(2500.0f), 10.0f, 15.0f),
    time(0), particleLifetime(30.0f), nParticles(8000), emitterPos(1, 0, 0), emitterDir(-1, 2, 0)
{
    gun = ObjMesh::load("media/pistol-with-engravings/source/colt.obj", false, true);
}

void SceneBasic_Uniform::initScene()
{
    compile();

    // Hide the cursor
    GLFWwindow* windowContext = glfwGetCurrentContext();
    glfwSetInputMode(windowContext, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST); // Enable depth testing

    // Set MVP matrices
    model = mat4(1.0f);
    view = mat4(1.0f);
    projection = mat4(1.0f);

    // Set spotlights radiance/intensity uniforms
    setSpotlightIntensity(10.0f); // 2500.0f

    // Set spotlights cutoff uniforms
    setSpotlightInnerCutoff(10.0f);
    setSpotlightOuterCutoff(15.0f);

    // Set misc uniforms
    hdrBloomProg.use();
    hdrBloomProg.setUniform("LumThresh", 5.0f); // 1.2f or 3.0f or 5.0f 
    hdrBloomProg.setUniform("Exposure", 0.35f);
    hdrBloomProg.setUniform("White", 0.982f);
    hdrBloomProg.setUniform("Gamma", 2.2f);
    hdrBloomProg.setUniform("BloomEnabled", bloomEnabled);

    pbrProg.use();
    pbrProg.setUniform("Gamma", 2.2f);

    // Setup skybox, gun textures
    setupTextures();

    // Setup FBO
    setupFBO();

    // Setup full screen quad for HDR and Bloom
    setupFullscreenQuad();

    // Compute weights and set uniforms for bloom blurring
    computeWeights();

    // Set up two sampler objects for linear and nearest filtering
    setupSamplers();

    // Set up everything to do with particles
    setupParticles();
}

void SceneBasic_Uniform::compile()
{
	try {
        // Compile Shaders
        // Skybox shader
        skyboxProg.compileShader("shader/skybox.vert");
        skyboxProg.compileShader("shader/skybox.frag");
        // PBR shader
        pbrProg.compileShader("shader/pbr.vert");
        pbrProg.compileShader("shader/pbr.frag");
        // HDR + Bloom shader
        hdrBloomProg.compileShader("shader/hdrBloom.vert");
        hdrBloomProg.compileShader("shader/hdrBloom.frag");
        // Particles shader
        particlesProg.compileShader("shader/particles.vert");
        particlesProg.compileShader("shader/particles.frag");

        // Link Shaders
        skyboxProg.link();
        pbrProg.link();
        hdrBloomProg.link();
        particlesProg.link();

        // Use pbr shader to begin
        pbrProg.use();
	} catch (GLSLProgramException &e) {
		cerr << e.what() << endl;
		exit(EXIT_FAILURE);
	}
}

void SceneBasic_Uniform::setSpotlightIntensity(float intensity)
{
    // Set spotlight objects
    if (whiteLightsEnabled)
    {
        spotlight.setIntensity(vec3(intensity));
    }
    else
    {
        spotlight.setIntensity(vec3(intensity, 0.0f, intensity));
    }

    // Set uniforms
    pbrProg.use();
    pbrProg.setUniform("Spotlight.L", spotlight.getIntensity());
}

void SceneBasic_Uniform::setSpotlightInnerCutoff(float degrees)
{
    spotlight.setInnerCutoff(degrees);
    pbrProg.use();
    pbrProg.setUniform("Spotlight.InnerCutoff", spotlight.getInnerCutoff());
}

void SceneBasic_Uniform::setSpotlightOuterCutoff(float degrees)
{
    spotlight.setOuterCutoff(degrees);
    pbrProg.use();
    pbrProg.setUniform("Spotlight.OuterCutoff", spotlight.getOuterCutoff());
}

void SceneBasic_Uniform::setupParticles()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    initBuffers();

    // The particle texture
    glActiveTexture(GL_TEXTURE8);
    particlesTexture = Texture::loadTexture("media/textures/fire_particles.png");
    glBindTexture(GL_TEXTURE_2D, particlesTexture);

    particlesProg.use();
    particlesProg.setUniform("ParticleTex", 8);
    particlesProg.setUniform("ParticleLifetime", particleLifetime);
    particlesProg.setUniform("ParticleSize", 0.05f);
    particlesProg.setUniform("Gravity", vec3(0.0f, -0.2f, 0.0f));
    particlesProg.setUniform("EmitterPos", emitterPos);
}

void SceneBasic_Uniform::setupTextures()
{
    // Load skybox texture
    GLuint skyboxTexture = Texture::loadHdrCubeMap("media/desert_skybox/desert");

    // Load gun textures
    gunAlbedoTexture = Texture::loadTexture("media/pistol-with-engravings/textures/BaseColor.png");
    gunNormalTexture = Texture::loadTexture("media/pistol-with-engravings/textures/Normal.png");
    gunMetallicTexture = Texture::loadTexture("media/pistol-with-engravings/textures/Metallic.png");
    gunRoughnessTexture = Texture::loadTexture("media/pistol-with-engravings/textures/Roughness.png");
    gunAOTexture = Texture::loadTexture("media/textures/white_1x1.png");

    // Load default textures
    defaultAlbedoTexture = Texture::loadTexture("media/textures/grey_1x1.png");
    defaultNormalTexture = Texture::loadTexture("media/textures/normal_up_1x1.png");
    defaultMetallicTexture = Texture::loadTexture("media/textures/black_1x1.png");
    defaultRoughnessTexture = Texture::loadTexture("media/textures/white_1x1.png");
    defaultAOTexture = Texture::loadTexture("media/textures/white_1x1.png");

    // Set active texture unit and bind loaded texture ids to 2D texture buffer
    bindPbrTextures(gunAlbedoTexture, gunNormalTexture, gunMetallicTexture, gunRoughnessTexture, gunAOTexture);

    // Set texture unit to 0 and bind cubemap
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
}

void SceneBasic_Uniform::bindPbrTextures(GLuint albedo, GLuint normal, GLuint metallic, GLuint roughness, GLuint ao)
{
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, albedo);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, normal);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, metallic);
    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_2D, roughness);
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D, ao);
}

void SceneBasic_Uniform::initBuffers()
{
    // Generate the buffers for initial velocity and start (birth) time
    glGenBuffers(1, &initVel); // Initial velocity buffer
    glGenBuffers(1, &startTime); // Start time buffer

    // Allocate space for all buffers
    int size = nParticles * sizeof(float);
    glBindBuffer(GL_ARRAY_BUFFER, initVel);
    glBufferData(GL_ARRAY_BUFFER, size * 3, 0, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, startTime);
    glBufferData(GL_ARRAY_BUFFER, size, 0, GL_STATIC_DRAW);

    // Fill the first velocity buffer with random velocities
    glm::mat3 emitterBasis = ParticleUtils::makeArbitraryBasis(emitterDir);
    vec3 v(0.0f);
    float velocity, theta, phi;
    std::vector<GLfloat> data(nParticles * 3);
    for (uint32_t i = 0; i < nParticles; i++) 
    {
        //pick the direction of the velocity
        theta = glm::mix(0.0f, glm::pi<float>() / 20.0f, randFloat());
        phi = glm::mix(0.0f, glm::two_pi<float>(), randFloat());

        v.x = sinf(theta) * cosf(phi);
        v.y = cosf(theta);
        v.z = sinf(theta) * sinf(phi);

        //scale to set the magnitude of the velocity
        velocity = glm::mix(1.25f, 1.5f, randFloat());
        v = glm::normalize(emitterBasis * v) * velocity;

        data[3 * i] = v.x;
        data[3 * i + 1] = v.y;
        data[3 * i + 2] = v.z;
    }

    glBindBuffer(GL_ARRAY_BUFFER, initVel);
    glBufferSubData(GL_ARRAY_BUFFER, 0, size * 3, data.data());

    // Fill the start time buffer
    float rate = particleLifetime / nParticles;
    for (int i = 0; i < nParticles; i++)
    {
        data[i] = rate * i;
    }
    glBindBuffer(GL_ARRAY_BUFFER, startTime);
    glBufferSubData(GL_ARRAY_BUFFER, 0, nParticles * sizeof(float), data.data());

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenVertexArrays(1, &particles);
    glBindVertexArray(particles);
    glBindBuffer(GL_ARRAY_BUFFER, initVel);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, startTime);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(1);

    glVertexAttribDivisor(0, 1);
    glVertexAttribDivisor(1, 1);

    glBindVertexArray(0);
}

float SceneBasic_Uniform::randFloat() 
{
    return rand.nextFloat();
}

void SceneBasic_Uniform::setupFBO() 
{
    setupHdrFBO();
    
    setupBlurFBO();
}

void SceneBasic_Uniform::setupHdrFBO()
{
    // Generate and bind the framebuffer
    glGenFramebuffers(1, &hdrFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

    // Create the texture object
    glGenTextures(1, &hdrTex);
    std::cout << "hdrTex has ID: " << hdrTex << std::endl;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, width, height);

    // Bind the texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrTex, 0);

    // Create the depth buffer. Not sure if we actually need this...
    GLuint depthBuf;
    glGenRenderbuffers(1, &depthBuf);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

    // Bind the depth buffer to the FBO
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuf);

    // Set the targets for the fragment output variables
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    // Unbind the framebuffer, and revert to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneBasic_Uniform::setupBlurFBO()
{
    // Create an FBO for the bright-pass filter and blur
    glGenFramebuffers(1, &blurFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo);

    // Create two texture objects to ping-pong for the bright-pass filter
    // and the two-pass blur
    bloomBufWidth = width / 8;
    bloomBufHeight = height / 8;

    glGenTextures(1, &tex1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, bloomBufWidth, bloomBufHeight);

    glGenTextures(1, &tex2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, tex2);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, bloomBufWidth, bloomBufHeight);

    // Bind tex1 to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

    // Unbind the framebuffer, and revert to default framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneBasic_Uniform::setupFullscreenQuad()
{
    // Array for full-screen quad
    GLfloat verts[] = {
    -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
    -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f, -1.0f, 1.0f, 0.0f
    };

    GLfloat tc[] = {
    0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f
    };

    // Set up the buffers
    unsigned int handle[2];
    glGenBuffers(2, handle);
    glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
    glBufferData(GL_ARRAY_BUFFER, 6 * 3 * sizeof(float), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, handle[1]);
    glBufferData(GL_ARRAY_BUFFER, 6 * 2 * sizeof(float), tc, GL_STATIC_DRAW);

    // Set up the vertex array object
    glGenVertexArrays(1, &fsQuad);
    glBindVertexArray(fsQuad);
    glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
    glVertexAttribPointer((GLuint)0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0); // Vertex position
    glBindBuffer(GL_ARRAY_BUFFER, handle[1]);
    glVertexAttribPointer((GLuint)2, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(2); // Texture coordinates
    glBindVertexArray(0);
}

void SceneBasic_Uniform::computeWeights()
{
    // Compute and sum the weights for bloom
    float weights[10], sum, sigma2 = 9.0f; // 25.0f
    weights[0] = gauss(0, sigma2);
    sum = weights[0];
    for (int i = 1; i < 10; i++) {
        weights[i] = gauss(float(i), sigma2);
        sum += 2 * weights[i];
    }

    // Normalize the weights and set the uniform
    for (int i = 0; i < 10; i++) {
        std::stringstream uniName;
        uniName << "Weight[" << i << "]";
        float val = weights[i] / sum;
        hdrBloomProg.use();
        hdrBloomProg.setUniform(uniName.str().c_str(), val);
    }
}

float SceneBasic_Uniform::gauss(float x, float sigma2)
{
    double coeff = 1.0 / (two_pi<float>() * sigma2);
    double exponent = -(x * x) / (2.0 * sigma2);
    return (float)(coeff * exp(exponent));
}

void SceneBasic_Uniform::setupSamplers()
{
    // Set up two sampler objects for linear and nearest filtering
    GLuint samplers[2];
    glGenSamplers(2, samplers);
    linearSampler = samplers[0];
    nearestSampler = samplers[1];
    GLfloat border[] = { 0.0f,0.0f,0.0f,0.0f };

    // Set up the nearest sampler
    glSamplerParameteri(nearestSampler, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(nearestSampler, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(nearestSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(nearestSampler, GL_TEXTURE_BORDER_COLOR, border);

    // Set up the linear sampler
    glSamplerParameteri(linearSampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(linearSampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glSamplerParameteri(linearSampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glSamplerParameterfv(linearSampler, GL_TEXTURE_BORDER_COLOR, border);

    // We want nearest sampling except for the last pass.
    glBindSampler(0, nearestSampler);
    glBindSampler(1, nearestSampler);
    glBindSampler(2, nearestSampler); // Seemingly an issue with sampler 2... maybe I should output it?
    std::cout << "This nearestSampler has ID: " << nearestSampler << std::endl;
}

void SceneBasic_Uniform::update( float t )
{
    time = t;

    float deltaT = t - tPrev;
    if (tPrev == 0.0f)
    {
        deltaT = 0.0f;
    }
    tPrev = t;
    
    // Increase angle for rotation
    angle += rotSpeed * deltaT;
    if (angle >= two_pi<float>())
    {
        angle -= two_pi<float>();
    }

    // Get current window context
    GLFWwindow* windowContext = glfwGetCurrentContext();

    // Handle keyboard input
    handleKeyboardInput(windowContext, deltaT);

    // Handle mouse input
    handleMouseMovement(windowContext, deltaT);
    handleMouseClicks(windowContext);

}

void SceneBasic_Uniform::handleKeyboardInput(GLFWwindow* windowContext, float deltaTime)
{
    if (glfwGetKey(windowContext, GLFW_KEY_W) == GLFW_PRESS) // Move forwards
    {
        cameraPosition += cameraSpeed * deltaTime * cameraForward;
    }
    if (glfwGetKey(windowContext, GLFW_KEY_S) == GLFW_PRESS) // Move backwards
    {
        cameraPosition -= cameraSpeed * deltaTime * cameraForward;
    }
    if (glfwGetKey(windowContext, GLFW_KEY_A) == GLFW_PRESS) // Move left
    {
        cameraPosition -= normalize(cross(cameraForward, cameraUp)) * cameraSpeed * deltaTime;
    }
    if (glfwGetKey(windowContext, GLFW_KEY_D) == GLFW_PRESS) // Move right
    {
        cameraPosition += normalize(cross(cameraForward, cameraUp)) * cameraSpeed * deltaTime;
    }
    if (glfwGetKey(windowContext, GLFW_KEY_SPACE) == GLFW_PRESS) // Move up
    {
        cameraPosition += cameraUp * cameraSpeed * deltaTime;
    }
    if (glfwGetKey(windowContext, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) // Move down
    {
        cameraPosition -= cameraUp * cameraSpeed * deltaTime;
    }
    if (glfwGetKey(windowContext, GLFW_KEY_2) == GLFW_PRESS) // Toggle rgb/white lights
    {
        whiteLightsEnabled = !whiteLightsEnabled;
        setSpotlightIntensity(10.0f);
    }
    if (glfwGetKey(windowContext, GLFW_KEY_3) == GLFW_PRESS) // Toggle bloom
    {
        bloomEnabled = !bloomEnabled;
        hdrBloomProg.use();
        hdrBloomProg.setUniform("BloomEnabled", bloomEnabled);
    }
}

void SceneBasic_Uniform::handleMouseMovement(GLFWwindow* windowContext, float deltaTime)
{
    // Get cursor position for camera movement
    double xpos = 0.0, ypos = 0.0;
    glfwGetCursorPos(windowContext, &xpos, &ypos);
    //Initially no last positions, so sets last positions to current positions
    if (mouseFirstEntry)
    {
        lastXPos = (float)xpos;
        lastYPos = (float)ypos;
        mouseFirstEntry = false;
    }

    //Sets values for change in position since last frame to current frame
    float xOffset = (float)xpos - lastXPos;
    float yOffset = lastYPos - (float)ypos;

    //Sets last positions to current positions for next frame
    lastXPos = (float)xpos;
    lastYPos = (float)ypos;

    //Moderates the change in position based on sensitivity value
    xOffset *= cameraSensitivity;
    yOffset *= cameraSensitivity;

    //Adjusts yaw & pitch values against changes in positions
    cameraYaw += xOffset;
    cameraPitch += yOffset;

    //Prevents turning up & down beyond 90 degrees to look backwards
    if (cameraPitch > 89.0f)
    {
        cameraPitch = 89.0f;
    }
    else if (cameraPitch < -89.0f)
    {
        cameraPitch = -89.0f;
    }

    //Modification of direction vector based on mouse turning
    vec3 direction;
    direction.x = cos(radians(cameraYaw)) * cos(radians(cameraPitch));
    direction.y = sin(radians(cameraPitch));
    direction.z = sin(radians(cameraYaw)) * cos(radians(cameraPitch));
    cameraForward = normalize(direction);
}

void SceneBasic_Uniform::handleMouseClicks(GLFWwindow* windowContext)
{
    if (glfwGetMouseButton(windowContext, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS)
    {
        // Shoot bullet
    }
    if (glfwGetMouseButton(windowContext, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        // Turn on ultraviolet light
    }
}

void SceneBasic_Uniform::render()
{
    pass1();
    computeLogAveLuminance();
    pass2();
    pass3();
    pass4();
    pass5();
}

void SceneBasic_Uniform::drawScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set normal matrix using model-view. Working in view/eye/camera space
    model = mat4(1.0f);
    mat4 mv = view * model;
    mat3 normalMatrix = mat3(transpose(inverse(mv)));

    // Set spotlight uniforms
    // Position
    spotlight.setPosition(vec4(cameraPosition, 1.0f));
    pbrProg.use();
    pbrProg.setUniform("Spotlight.Position", view * spotlight.getPosition());

    // Direction
    spotlight.setDirection(cameraForward);
    pbrProg.use();
    pbrProg.setUniform("Spotlight.Direction", vec3(view * vec4(spotlight.getDirection(), 0.0f)));

    // Skybox rendering
    skyboxProg.use();

    model = mat4(1.0f);
    mat4 prevView = view;
    view = lookAt(vec3(0.0f), cameraForward, cameraUp); // For infinite skybox

    setMatrices(skyboxProg);
    skybox.render();

    view = prevView; // Back to normal

    // Plane rendering
    pbrProg.use();

    // Set camera position
    pbrProg.setUniform("CameraPos", view * vec4(cameraPosition, 1.0f));

    // Set plane model matrix
    model = mat4(1.0f);
    model = translate(model, vec3(0.0f, -10.0f, 0.0f));

    // Bind default, set MVP matrix uniforms and render plane
    bindPbrTextures(defaultAlbedoTexture, defaultNormalTexture, defaultMetallicTexture, defaultRoughnessTexture, defaultAOTexture);
    setMatrices(pbrProg);
    plane.render();

    // Particles rendering
    model = mat4(1.0f);
    glDepthMask(GL_FALSE);
    particlesProg.use();
    setMatrices(particlesProg);
    particlesProg.setUniform("Time", time);
    glBindVertexArray(particles);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, nParticles);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);

    // Gun rendering
    pbrProg.use();

    // Set camera position
    pbrProg.setUniform("CameraPos", view * vec4(cameraPosition, 1.0f));

    // Set gun model matrix
    model = mat4(1.0f);

    /*model = scale(model, vec3(3.0f)); // Don't need this yet - wait for gamification
    model = translate(model, cameraPosition);
    model = translate(model, -1.0f * cameraUp);
    model = translate(model, 1.0f * normalize(cross(cameraForward, cameraUp)));*/

    //model = rotate(model, radians(180.0f), vec3(0.0f, 1.0f, 0.0f)); // Really old stuff from CW1
    //model = rotate(model, radians(-90.0f), vec3(0.0f, 0.0f, 1.0f));
    //model = scale(model, vec3(0.05f));

    // Bind gun textures, set MVP matrix uniforms and render gun
    bindPbrTextures(gunAlbedoTexture, gunNormalTexture, gunMetallicTexture, gunRoughnessTexture, gunAOTexture);
    setMatrices(pbrProg);
    gun->render();
}

void SceneBasic_Uniform::pass1() // Draw the scene normally
{
    pbrProg.use();
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    view = lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp);
    projection = glm::perspective(glm::radians(70.0f), (float)width / height, 0.3f, 1000.0f);

    drawScene();
}

void SceneBasic_Uniform::computeLogAveLuminance()
{
    int size = width * height;
    std::vector<GLfloat> texData(size * 3);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, texData.data());
    float sum = 0.0f;
    for (int i = 0; i < size; i++)
    {
        float lum = dot(vec3(texData[i * 3 + 0], texData[i * 3 + 1], texData[i * 3 + 2]), vec3(0.2126f, 0.7152f, 0.0722f));
        if (!std::isfinite(lum)) continue;
        sum += logf(lum + 0.00001f);
    }
    hdrBloomProg.use();
    hdrBloomProg.setUniform("AveLum", expf(sum / size));
}

void SceneBasic_Uniform::pass2() // Draw the blur
{
    hdrBloomProg.use();
    hdrBloomProg.setUniform("Pass", 2);
    glBindFramebuffer(GL_FRAMEBUFFER, blurFbo);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);
    glViewport(0, 0, bloomBufWidth, bloomBufHeight);
    glDisable(GL_DEPTH_TEST);

    glClearColor(0, 0, 0, 0);

    glClear(GL_COLOR_BUFFER_BIT);

    model = mat4(1.0f);
    view = mat4(1.0f);
    projection = mat4(1.0f);

    setMatrices(hdrBloomProg);

    glBindVertexArray(fsQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SceneBasic_Uniform::pass3()
{
    hdrBloomProg.use();
    hdrBloomProg.setUniform("Pass", 3);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);

    glBindVertexArray(fsQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void SceneBasic_Uniform::pass4()
{
    hdrBloomProg.use();
    hdrBloomProg.setUniform("Pass", 4);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

    glBindVertexArray(fsQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void SceneBasic_Uniform::pass5()
{
    hdrBloomProg.use();
    hdrBloomProg.setUniform("Pass", 5);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, width, height);

    glBindSampler(1, linearSampler);
    glBindVertexArray(fsQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glBindSampler(1, nearestSampler);
}

void SceneBasic_Uniform::resize(int w, int h)
{
    glViewport(0, 0, w, h);
    width = w;
    height = h;
    projection = glm::perspective(glm::radians(70.0f), (float)w / h, 0.3f, 1000.0f);
}

void SceneBasic_Uniform::setMatrices(GLSLProgram& p)
{
    glm::mat4 mv = view * model;
    p.setUniform("ModelMatrix", model);
    p.setUniform("ModelViewMatrix", mv);
    p.setUniform("ProjectionMatrix", projection);
    p.setUniform("MVP", projection * mv);
    p.setUniform("NormalMatrix", mat3(transpose(inverse(mv))));
}