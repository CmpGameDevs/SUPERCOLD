#include <ibl/bloom-buffer.hpp>
#include <shader/shader.hpp>
#include <ibl/hdr-system.hpp>
#include <ibl/fullscreenquad.hpp>
#include <material/material.hpp>
#include <glad/gl.h>
#include <vector>
#include <algorithm>

enum BloomDirection {
    BOTH = 0,
    HORIZONTAL = 1,
    VERTICAL = 2
};

namespace our{

    class PostProcess
    {

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

        ShaderProgram* bloomShader;

        GLuint postprocessFrameBuffer , bloomColorTexture;
        Texture2D *colorTarget, *depthTarget;
        TexturedMaterial* postprocessMaterial;

        FullscreenQuad* fullscreenQuad;

    private:
        void renderBloom();
        void createBloom();

    public:
        void init(const glm::ivec2& windowSize, const nlohmann::json& config);
        void destroy();
        void bind();
        void unbind();
        void renderPostProcess();
        float getBloomBrightnessCutoff() const { return bloomBrightnessCutoff; }    

    };
}