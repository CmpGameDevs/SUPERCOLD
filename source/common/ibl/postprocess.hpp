#include <algorithm>
#include <glad/gl.h>
#include <ibl/bloom-buffer.hpp>
#include <ibl/fullscreenquad.hpp>
#include <ibl/hdr-system.hpp>
#include <material/material.hpp>
#include <shader/shader.hpp>
#include <vector>

enum BloomDirection { BOTH = 0, HORIZONTAL = 1, VERTICAL = 2 };

namespace our {

class PostProcess {

    glm::ivec2 windowSize;

    BloomFramebuffer *bloomBuffers[2];
    unsigned int bloomFramebufferResult;
    bool bloomEnabled = true;
    float bloomIntensity = 1.0;
    int bloomIterations = 10;
    int bloomDirection = BloomDirection::BOTH;
    bool tonemappingEnabled = false;
    float gammaCorrectionFactor = 2.2;
    float bloomBrightnessCutoff = 1.0;

    ShaderProgram *bloomShader;

    GLuint postprocessFrameBuffer, bloomColorTexture;
    Texture2D *colorTarget, *depthTarget;
    TexturedMaterial *postprocessMaterial;

    FullscreenQuad *fullscreenQuad;

    // Vignette effect parameters
    bool vignetteEnabled = false;
    float vignetteIntensity = 0.5f;
    glm::vec3 vignetteColor = glm::vec3(0.0f, 0.0f, 0.0f);
    Texture2D *freezeFrameTexture = nullptr;
    Sampler *freezeFrameSampler = nullptr;

    // Motion blur parameters
    bool motionBlurEnabled = false;
    float motionBlurStrength = 0.5f;
    int motionBlurSamples = 5;
    Texture2D *previousFrameTexture = nullptr;
    Sampler *previousFrameSampler = nullptr;

    // Direction and velocity of motion blur (can be set based on camera movement)
    glm::vec2 motionDirection = glm::vec2(1.0f, 0.0f); // Default horizontal

    // For dynamic effect control
    std::unordered_map<std::string, float *> effectParameters;

    // View-projection matrices for motion blur
    glm::mat4 viewProjectionMatrix;
    glm::mat4 previousViewProjectionMatrix;

  private:
    void renderBloom();
    void createBloom();

  public:
    void init(const glm::ivec2 &windowSize, const nlohmann::json &config);
    void destroy();
    void bind();
    void unbind();
    void renderPostProcess();
    float getBloomBrightnessCutoff() const { return bloomBrightnessCutoff; }

    // Method to get/set effect parameters at runtime
    void setEffectParameter(const std::string &paramName, float value);
    float getEffectParameter(const std::string &paramName) const;

    // Accessor methods for blur parameters
    void setMotionBlurEnabled(bool enabled) { motionBlurEnabled = enabled; }
    bool isMotionBlurEnabled() const { return motionBlurEnabled; }
    void setMotionBlurStrength(float strength) { motionBlurStrength = strength; }
    float getMotionBlurStrength() const { return motionBlurStrength; }
    void setMotionBlurSamples(int samples) { motionBlurSamples = samples; }
    int getMotionBlurSamples() const { return motionBlurSamples; }
    void setMotionDirection(const glm::vec2 &direction) { motionDirection = glm::normalize(direction); }

    // Accessor methods for view-projection matrices
    void setViewProjectionMatrix(const glm::mat4 &vp) {
        previousViewProjectionMatrix = viewProjectionMatrix;
        viewProjectionMatrix = vp;
    }

    void updatePreviousViewProjectionMatrix() { previousViewProjectionMatrix = viewProjectionMatrix; }
};
} // namespace our