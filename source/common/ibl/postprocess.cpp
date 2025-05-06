#include "ibl/postprocess.hpp"

namespace our {

void PostProcess::init(const glm::ivec2 &windowSize, const nlohmann::json &config) {
    // Store the window size for later use
    this->windowSize = windowSize;

    postprocessFrameBuffer = 0;

    bloomEnabled = config.value("bloomEnabled", false);
    bloomIntensity = config.value("bloomIntensity", 1.0f);
    bloomIterations = config.value("bloomIterations", 10);
    bloomDirection = config.value("bloomDirection", BloomDirection::BOTH);
    tonemappingEnabled = config.value("tonemappingEnabled", false);
    gammaCorrectionFactor = config.value("gammaCorrectionFactor", 2.2f);
    bloomBrightnessCutoff = config.value("bloomBrightnessCutoff", 1.0f);

    // Add vignette effect parameters
    vignetteEnabled = config.value("vignetteEnabled", false);
    vignetteIntensity = config.value("vignetteIntensity", 0.5f);

    // Add motion blur parameters from config
    motionBlurEnabled = config.value("motionBlurEnabled", false);
    motionBlurStrength = config.value("motionBlurStrength", 0.5f);
    motionBlurSamples = config.value("motionBlurSamples", 5);

    effectParameters["bloomIntensity"] = &bloomIntensity;
    effectParameters["vignetteIntensity"] = &vignetteIntensity;
    effectParameters["gammaCorrectionFactor"] = &gammaCorrectionFactor;
    effectParameters["motionBlurStrength"] = &motionBlurStrength;

    // Parse vignette color if provided
    if (config.contains("vignetteColor")) {
        auto colorConfig = config["vignetteColor"];
        if (colorConfig.is_array() && colorConfig.size() == 3) {
            vignetteColor =
                glm::vec3(colorConfig[0].get<float>(), colorConfig[1].get<float>(), colorConfig[2].get<float>());
        }
    }

    // Parse motion direction if provided
    if (config.contains("motionDirection")) {
        auto dirConfig = config["motionDirection"];
        if (dirConfig.is_array() && dirConfig.size() == 2) {
            motionDirection = glm::vec2(dirConfig[0].get<float>(), dirConfig[1].get<float>());
            if (glm::length(motionDirection) > 0.01f) {
                motionDirection = glm::normalize(motionDirection);
            }
        }
    }

    if (motionBlurEnabled) {
        previousFrameTexture = new Texture2D();
        previousFrameTexture->bind();
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, windowSize.x, windowSize.y);

        previousFrameSampler = new Sampler();
        previousFrameSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        previousFrameSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        previousFrameSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        previousFrameSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    // Parse Freeze texture
    if (config.contains("freezeFrameTexture")) {
        auto freezeFrameConfig = config["freezeFrameTexture"];
        if (freezeFrameConfig.is_string()) {
            freezeFrameTexture = our::AssetLoader<our::Texture2D>::get(freezeFrameConfig.get<std::string>());

            freezeFrameSampler = new Sampler();
            freezeFrameSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            freezeFrameSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            freezeFrameSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            freezeFrameSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }

    // Create the bloom shader
    bloomShader = our::AssetLoader<our::ShaderProgram>::get("bloom");

    // TODO: (Req 11) Create a framebuffer
    glGenFramebuffers(1, &postprocessFrameBuffer);

    // TODO: (Req 11) Create a color and a depth texture and attach them to the framebuffer
    //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
    //  The depth format can be (Depth component with 24 bits).
    colorTarget = new Texture2D();
    colorTarget->bind();

    GLsizei levelsCnt = (GLsizei)glm::floor(glm::log2((float)glm::max(windowSize.x, windowSize.y))) + 1;
    glTexStorage2D(GL_TEXTURE_2D, levelsCnt, GL_RGBA8, windowSize.x, windowSize.y);

    depthTarget = new Texture2D();
    depthTarget->bind();

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, windowSize.x, windowSize.y);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTarget->getOpenGLName(), 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTarget->getOpenGLName(), 0);

    // create bloom texture
    if (bloomEnabled) {
        createBloom();
    }

    // allow to draw to both color attachments
    unsigned int colorAttachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, colorAttachments);

    // TODO: (Req 11) Unbind the framebuffer just to be safe
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

    //  // Create a vertex array to use for drawing the texture
    // ! WE NOW DO THIS IN THE FULLSCREEN QUAD CLASS
    //  glGenVertexArrays(1, &postProcessVertexArray);

    // Create a sampler to use for sampling the scene texture in the post processing shader
    Sampler *postprocessSampler = new Sampler();
    postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create the post processing shader
    ShaderProgram *postprocessShader = our::AssetLoader<our::ShaderProgram>::get("postprocess");

    // Create a post processing material
    postprocessMaterial = new TexturedMaterial();
    postprocessMaterial->shader = postprocessShader;
    postprocessMaterial->texture = colorTarget;
    postprocessMaterial->sampler = postprocessSampler;
    // The default options are fine but we don't need to interact with the depth buffer
    // so it is more performant to disable the depth mask
    postprocessMaterial->pipelineState.depthMask = false;

    // Initialize the fullscreen quad
    fullscreenQuad = new FullscreenQuad();
}

void PostProcess::createBloom() {
    // Initialize bloom buffers and framebuffer
    bloomBuffers[0] = new BloomFramebuffer(windowSize.x, windowSize.y);
    bloomBuffers[1] = new BloomFramebuffer(windowSize.x, windowSize.y);
    bloomBuffers[0]->init();
    bloomBuffers[1]->init();
    bloomFramebufferResult = 0;
    bloomColorTexture = 0;

    // Create a bloom texture
    glGenTextures(1, &bloomColorTexture);
    glBindTexture(GL_TEXTURE_2D, bloomColorTexture);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, windowSize.x, windowSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, bloomColorTexture, 0);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void PostProcess::destroy() {
    // Delete all objects related to post processing
    if (postprocessMaterial) {
        glDeleteFramebuffers(1, &postprocessFrameBuffer);
        delete colorTarget;
        delete depthTarget;
        delete postprocessMaterial->sampler;
        delete postprocessMaterial;
    }

    // Delete bloom buffers
    for (int i = 0; i < 2; ++i) {
        delete bloomBuffers[i];
    }

    // delete prev
    if (previousFrameTexture) {
        delete previousFrameTexture;
        delete previousFrameSampler;
    }

    // delete freeze frame texture
    if (freezeFrameTexture) {
        delete freezeFrameTexture;
        delete freezeFrameSampler;
    }
}

void PostProcess::bind() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postprocessFrameBuffer);
    unsigned int colorAttachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, colorAttachments);
}

void PostProcess::unbind() {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

void PostProcess::renderBloom() {

    glViewport(0, 0, windowSize.x, windowSize.y);

    // Define blur directions
    glm::vec2 blurDirectionX = glm::vec2(1.0f, 0.0f);
    glm::vec2 blurDirectionY = glm::vec2(0.0f, 1.0f);

    // Adjust blur directions based on bloomDirection
    if (bloomDirection == BloomDirection::HORIZONTAL) {
        blurDirectionY = blurDirectionX;
    } else if (bloomDirection == BloomDirection::VERTICAL) {
        blurDirectionX = blurDirectionY;
    }

    // use texture unit 0 for bloomColorTexture
    glActiveTexture(GL_TEXTURE0);
    // bind the bloom color texture
    glBindTexture(GL_TEXTURE_2D, bloomColorTexture);
    // generate mipmaps for the bloom color texture
    glGenerateMipmap(GL_TEXTURE_2D);

    bloomShader->use();
    bloomShader->set("inputColorTexture", 0);

    for (auto mipLevel = 0; mipLevel <= 2; mipLevel++) {
        bloomBuffers[0]->setMipLevel(mipLevel);
        bloomBuffers[1]->setMipLevel(mipLevel);

        // First pass (horizontal)
        bloomBuffers[0]->bind();
        glBindTexture(GL_TEXTURE_2D, bloomColorTexture);
        bloomShader->set("sampleMipLevel", mipLevel);
        bloomShader->set("blurDirection", blurDirectionX); // Always use X first
        fullscreenQuad->Draw();

        // Second pass (vertical) - applied to the result of horizontal
        bloomBuffers[1]->bind();
        glBindTexture(GL_TEXTURE_2D, bloomBuffers[0]->getColorTextureId());
        bloomShader->set("blurDirection", blurDirectionY); // Then use Y
        fullscreenQuad->Draw();

        // Now bloomBuffers[1] contains a proper circular blur

        // Additional iterations (apply more blur passes if needed)
        unsigned int bloomFramebuffer = 0;

        for (auto i = 1; i < bloomIterations; i++) {
            // Horizontal pass
            bloomBuffers[bloomFramebuffer]->bind();
            glBindTexture(GL_TEXTURE_2D, bloomBuffers[1]->getColorTextureId());
            bloomShader->set("blurDirection", blurDirectionX);
            fullscreenQuad->Draw();

            // Vertical pass
            bloomBuffers[1]->bind();
            glBindTexture(GL_TEXTURE_2D, bloomBuffers[bloomFramebuffer]->getColorTextureId());
            bloomShader->set("blurDirection", blurDirectionY);
            fullscreenQuad->Draw();
        }

        // The final result is always in bloomBuffers[1]
        bloomFramebufferResult = 1;
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

void PostProcess::renderPostProcess() {

    if (bloomEnabled) {
        renderBloom();
    }

    // TODO: (Req 11) Return to the default framebuffer
    glViewport(0, 0, windowSize.x, windowSize.y);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // TODO: (Req 11) Setup the postprocess material and draw the fullscreen triangle
    postprocessMaterial->setup();
    postprocessMaterial->shader->set("bloomEnabled", bloomEnabled);
    postprocessMaterial->shader->set("bloomIntensity", bloomIntensity);
    postprocessMaterial->shader->set("tonemappingEnabled", tonemappingEnabled);
    postprocessMaterial->shader->set("gammaCorrectionFactor", gammaCorrectionFactor);

    // Vintage
    postprocessMaterial->shader->set("vignetteEnabled", vignetteEnabled);
    postprocessMaterial->shader->set("vignetteIntensity", vignetteIntensity);
    postprocessMaterial->shader->set("vignetteColor", vignetteColor);
    postprocessMaterial->shader->set("screenSize", glm::vec2(windowSize.x, windowSize.y));

    // Set motion blur parameters
    postprocessMaterial->shader->set("motionBlurEnabled", motionBlurEnabled);
    postprocessMaterial->shader->set("motionBlurStrength", motionBlurStrength);
    postprocessMaterial->shader->set("motionBlurSamples", motionBlurSamples);
    postprocessMaterial->shader->set("motionDirection", motionDirection);

    // Set matrices for depth-based motion blur
    postprocessMaterial->shader->set("viewProjectionMatrix", viewProjectionMatrix);
    postprocessMaterial->shader->set("previousViewProjectionMatrix", previousViewProjectionMatrix);
    postprocessMaterial->shader->set("viewProjectionInverseMatrix", glm::inverse(viewProjectionMatrix));

    // Bind depth texture for motion blur
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, depthTarget->getOpenGLName());
    postprocessMaterial->shader->set("depthTexture", 4);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, colorTarget->getOpenGLName());
    postprocessMaterial->shader->set("colorTexture", 0);

    if (bloomEnabled) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomBuffers[bloomFramebufferResult]->getColorTextureId());
        postprocessMaterial->shader->set("bloomTexture", 1);
    }

    // Bind previous frame texture for motion blur
    if (motionBlurEnabled && previousFrameTexture) {
        glActiveTexture(GL_TEXTURE2);
        previousFrameTexture->bind();
        if (previousFrameSampler)
            previousFrameSampler->bind(2);
        postprocessMaterial->shader->set("previousFrameTexture", 2);
    }

    // Bind freeze frame texture if available and enabled
    if (freezeFrameTexture) {
        glActiveTexture(GL_TEXTURE3);
        freezeFrameTexture->bind();
        if (freezeFrameSampler)
            freezeFrameSampler->bind(3);
        postprocessMaterial->shader->set("freezeFrameTexture", 3);
        postprocessMaterial->shader->set("hasFrameTexture", true);
    } else {
        postprocessMaterial->shader->set("hasFrameTexture", false);
    }

    // Store current frame for next frame's motion blur
    if (motionBlurEnabled && previousFrameTexture) {
        // Create a framebuffer to capture the current frame
        GLuint tempFramebuffer;
        glGenFramebuffers(1, &tempFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tempFramebuffer);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               previousFrameTexture->getOpenGLName(), 0);

        if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Error: Temporary framebuffer for motion blur is not complete!" << std::endl;
            // Handle the error appropriately, maybe return or throw an exception
        }

        // Copy from default framebuffer to our texture
        glBlitFramebuffer(0, 0, windowSize.x, windowSize.y, 0, 0, windowSize.x, windowSize.y, GL_COLOR_BUFFER_BIT,
                          GL_NEAREST);

        // Clean up
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &tempFramebuffer);
    }
    // ! WE NOW DO THIS IN THE FULLSCREEN QUAD CLASS
    // glBindVertexArray(postProcessVertexArray);
    // glDrawArrays(GL_TRIANGLES, 0, 3);

    fullscreenQuad->Draw();
}

void PostProcess::setEffectParameter(const std::string &paramName, float value) {
    auto it = effectParameters.find(paramName);
    if (it != effectParameters.end()) {
        *(it->second) = value;
    }
}

float PostProcess::getEffectParameter(const std::string &paramName) const {
    auto it = effectParameters.find(paramName);
    if (it != effectParameters.end()) {
        return *(it->second);
    }
    return 0.0f;
}

} // namespace our
