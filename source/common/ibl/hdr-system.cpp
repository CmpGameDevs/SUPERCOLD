
#include "hdr-system.hpp"

namespace our
{

    void our::HDRSystem::Initialize(){
        initializeShader();
        initializeTexture();
        initializeBuffer();
        initializeCubeMap();
    }

    void our::HDRSystem::initializeShader()
    {
        equirectangular_shader = AssetLoader<ShaderProgram>::get("equirectangular");
        irradiance_shader = AssetLoader<ShaderProgram>::get("irradiance");
        background_shader = AssetLoader<ShaderProgram>::get("background");
        brdf_shader = AssetLoader<ShaderProgram>::get("brdf");
        prefilter_shader = AssetLoader<ShaderProgram>::get("prefilter");
    }
    
    void our::HDRSystem::initializeCubeMap()
    {
        equirectangularCubeMap = new EquiRectangularCubeMap(equirectangular_shader, envCubeMap, cubeMapBuffer);
        irradianceCubeMap = new IrradianceCubeMap(irradiance_shader, irradianceMap, cubeMapBuffer);
        prefilterCubeMap = new PrefilterCubeMap(prefilter_shader, prefilterMap, cubeMapBuffer);
    }      

    void our::HDRSystem::initializeBuffer()
    {
        cubeMapBuffer = new CubeMapBuffer();
    }

    void our::HDRSystem::initializeTexture(){
        envCubeMap = new CubeMapTexture();
        irradianceMap = new CubeMapTexture();
        prefilterMap = new CubeMapTexture();
    }

    void our::HDRSystem::setup(){
        background_shader->use();
        background_shader->set("environmentMap", TEXTURE_UNIT_ENVIRONMENT);

        our::Texture2D* hdr_texture = our::texture_utils::loadHDR("assets/textures/hdr/circus_backstage.hdr", false);

        cubeMapBuffer->size = { 512, 512 };
        cubeMapBuffer->setupFrameBuffer();
        cubeMapBuffer->setupRenderBuffer();

        envCubeMap->setupCubeTexture(cubeMapBuffer->size);

         // pbr: convert HDR equirectangular environment map to cubemap equivalent
        // ----------------------------------------------------------------------
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_HDR);
        hdr_texture->bind();

        equirectangularCubeMap->convertToCubeMap(TEXTURE_UNIT_HDR, captureProjection, captureViews);

        // then let OpenGL generate mipmaps from first mip face (combatting visible dots artifact)
        envCubeMap->generateMipmaps();

        // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
        // --------------------------------------------------------------------------------

        cubeMapBuffer->size = { 32, 32 };

        irradianceMap->setupCubeTexture(cubeMapBuffer->size);

        cubeMapBuffer->bind();
        cubeMapBuffer->setRenderBufferStorage(GL_DEPTH_COMPONENT24, cubeMapBuffer->size.x, cubeMapBuffer->size.y);

        // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
        // -----------------------------------------------------------------------------
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ENVIRONMENT);
        envCubeMap->bind();

        irradianceCubeMap->convertToCubeMap(TEXTURE_UNIT_ENVIRONMENT, captureProjection, captureViews);


        // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
        // --------------------------------------------------------------------------------
        cubeMapBuffer->size = { 128, 128 };

        prefilterMap->setupCubeTexture(cubeMapBuffer->size, GL_RGB16F, GL_RGB, GL_FLOAT, true);

        // pbr: run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
        // ----------------------------------------------------------------------------------------------------
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_PREFILTER);
        envCubeMap->bind();

        prefilterCubeMap->convertToCubeMap(TEXTURE_UNIT_PREFILTER, captureProjection, captureViews);

        // pbr: generate a 2D LUT from the BRDF equations used.
        // ----------------------------------------------------
        // 1. pre-allocate a texture for the BRDF lookup texture atlas.

        cubeMapBuffer->size = { 512, 512 };

        brdfLUTTexture = new our::Texture2D();
        brdfLUTTexture->bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, cubeMapBuffer->size.x, cubeMapBuffer->size.y, 0, GL_RG, GL_FLOAT, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.

        cubeMapBuffer->bind();
        cubeMapBuffer->setRenderBufferStorage(GL_DEPTH_COMPONENT24, cubeMapBuffer->size.x, cubeMapBuffer->size.y);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, brdfLUTTexture->getOpenGLName(), 0);

        glViewport(0, 0, cubeMapBuffer->size.x, cubeMapBuffer->size.y);

        brdf_shader->use();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        our::mesh_utils::renderQuad();

        cubeMapBuffer->unbindFrameBuffer();

    }

    void our::HDRSystem::bindTextures()
    {
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_IRRADIANCE);
        irradianceMap->bind();
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_PREFILTER);
        prefilterMap->bind();
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_BRDF);
        brdfLUTTexture->bind();
    }

    void our::HDRSystem::unbindTextures()
    {
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_IRRADIANCE);
        irradianceMap->unbind();
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_PREFILTER);
        prefilterMap->unbind();
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_BRDF);
        brdfLUTTexture->unbind();
    }

    void our::HDRSystem::renderBackground(glm::mat4 projection, glm::mat4 view)
    {
        glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ENVIRONMENT);
        envCubeMap->bind();
        background_shader->use();
        background_shader->set("projection", projection);
        background_shader->set("view", view);
        our::mesh_utils::renderCube();
    }

}
