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
    tPrev(0),
    angle(0.0f),
    rotSpeed(pi<float>() / 8.0f),
    whiteLightsEnabled(true),
    bloomEnabled(true),
    cameraPosition(0.0f, 0.0f, 10.0f),
    cameraForward(0.0f, 0.0f, 1.0f),
    cameraUp(0.0f, 1.0f, 0.0f),
    cameraYaw(-90.0f),
    cameraPitch(0.0f),
    cameraSpeed(5.0f),
    cameraSensitivity(0.025f),
    mouseFirstEntry(true),
    lastXPos(width / 2.0f),
    lastYPos(height / 2.0f)
{
    //gun = ObjMesh::load("media/38-special-revolver/source/rev_anim.obj.obj", false, true);
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
    setSpotlightsIntensity(2500.0f);

    // Set spotlights cutoff uniforms
    setSpotlightsInnerCutoff(10.0f);
    setSpotlightsOuterCutoff(15.0f);

    // Set misc uniforms
    hdrBloomProg.use();
    hdrBloomProg.setUniform("LumThresh", 3.0f);
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
        

        // Link Shaders
        skyboxProg.link();
        pbrProg.link();
        hdrBloomProg.link();

        // Use pbr shader to begin
        pbrProg.use();
	} catch (GLSLProgramException &e) {
		cerr << e.what() << endl;
		exit(EXIT_FAILURE);
	}
}

void SceneBasic_Uniform::update( float t )
{
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

void SceneBasic_Uniform::render()
{
    pass1();
    computeLogAveLuminance();
    pass2();
    pass3();
    pass4();
    pass5();
}

void SceneBasic_Uniform::pass1() // Draw the scene normally
{
    hdrBloomProg.use();
    hdrBloomProg.setUniform("Pass", 1);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glViewport(0, 0, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);

    view = lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp);
    projection = glm::perspective(glm::radians(70.0f), (float)width / height, 0.3f, 100.0f);

    drawScene();
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
    //glBindVertexArray(0);
}

void SceneBasic_Uniform::pass3()
{
    hdrBloomProg.use();
    hdrBloomProg.setUniform("Pass", 3);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex2, 0);

    glBindVertexArray(fsQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
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
    //glBindVertexArray(0);
    glBindSampler(1, nearestSampler);
}

void SceneBasic_Uniform::drawScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Set normal matrix using view
    model = mat4(1.0f);
    mat4 mv = view * model;
    mat3 normalMatrix = mat3(transpose(inverse(mv))); //mat3 normalMatrix = mat3(vec3(view[0]), vec3(view[1]), vec3(view[2]));

    // Set spotlight uniforms
    for (int i = 0; i < 3; i++)
    {
        // Position
        std::stringstream positionName;
        positionName << "Spotlights[" << i << "].Position";
        float x = 50.0f * sin(angle + (two_pi<float>() / 3) * i);
        float z = 50.0f * cos(angle + (two_pi<float>() / 3) * i);
        vec4 lightPos = vec4(x, 15.0f, z, 1.0f);
        pbrProg.use();
        pbrProg.setUniform(positionName.str().c_str(), view * lightPos);

        // Direction
        std::stringstream directionName;
        directionName << "Spotlights[" << i << "].Direction";
        pbrProg.use();
        //pbrProg.setUniform(directionName.str().c_str(), normalMatrix * vec3(-lightPos.x, -lightPos.y, -lightPos.z)); // -lightPos.x, y and z
        pbrProg.setUniform(directionName.str().c_str(), vec3(view * vec4(-lightPos.x, -lightPos.y, -lightPos.z, 0.0f))); // I think this should be in view space...
    }

    // Skybox rendering
    skyboxProg.use();

    model = mat4(1.0f);
    view = lookAt(vec3(0.0f), cameraForward, cameraUp); // For infinite skybox

    setMatrices(skyboxProg);
    skybox.render();

    view = lookAt(cameraPosition, cameraPosition + cameraForward, cameraUp); // Back to normal

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

void SceneBasic_Uniform::resize(int w, int h)
{
    glViewport(0, 0, w, h);
    width = w;
    height = h;
    projection = glm::perspective(glm::radians(70.0f), (float)w / h, 0.3f, 100.0f);
}

void SceneBasic_Uniform::setMatrices(GLSLProgram& p)
{
    glm::mat4 mv = view * model;
    p.setUniform("ModelMatrix", model);
    p.setUniform("ModelViewMatrix", mv);
    p.setUniform("MVP", projection * mv);
    p.setUniform("NormalMatrix", transpose(inverse(mat3(vec3(mv[0]), vec3(mv[1]), vec3(mv[2])))));
}

void SceneBasic_Uniform::setSpotlightsIntensity(float intensity)
{
    if (whiteLightsEnabled)
    {
        // Gun program
        pbrProg.use();
        pbrProg.setUniform("Spotlights[0].L", vec3(intensity));
        pbrProg.setUniform("Spotlights[1].L", vec3(intensity));
        pbrProg.setUniform("Spotlights[2].L", vec3(intensity));
    }
    else
    {
        // Gun program
        pbrProg.use();
        pbrProg.setUniform("Spotlights[0].L", vec3(intensity, 0.0f, 0.0f));
        pbrProg.setUniform("Spotlights[1].L", vec3(0.0f, intensity, 0.0f));
        pbrProg.setUniform("Spotlights[2].L", vec3(0.0f, 0.0f, intensity));
    }
}

void SceneBasic_Uniform::setSpotlightsInnerCutoff(float degrees)
{
    for (int i = 0; i < 3; i++)
    {
        std::stringstream cutoffName;
        cutoffName << "Spotlights[" << i << "].InnerCutoff";
        pbrProg.use();
        pbrProg.setUniform(cutoffName.str().c_str(), radians(degrees));
    }
}

void SceneBasic_Uniform::setSpotlightsOuterCutoff(float degrees)
{
    for (int i = 0; i < 3; i++)
    {
        std::stringstream cutoffName;
        cutoffName << "Spotlights[" << i << "].OuterCutoff";
        pbrProg.use();
        pbrProg.setUniform(cutoffName.str().c_str(), radians(degrees));
    }
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

void SceneBasic_Uniform::setupFBO() {
    // Generate and bind the framebuffer
    glGenFramebuffers(1, &hdrFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFbo);

    // Create the texture object
    glGenTextures(1, &hdrTex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrTex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB32F, width, height);

    // Bind the texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, hdrTex, 0);

    // Create the depth buffer
    GLuint depthBuf;
    glGenRenderbuffers(1, &depthBuf);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuf);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);

    // Bind the depth buffer to the FBO
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuf);

    // Set the targets for the fragment output variables
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
    glDrawBuffers(1, drawBuffers);

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
    glBindSampler(2, nearestSampler);
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
        setSpotlightsIntensity(2500.0f);
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

float SceneBasic_Uniform::gauss(float x, float sigma2)
{
    double coeff = 1.0 / (two_pi<float>() * sigma2);
    double exponent = -(x * x) / (2.0 * sigma2);
    return (float)(coeff * exp(exponent));
}