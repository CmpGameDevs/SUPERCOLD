#include "cubemap.hpp"

namespace our
{

    void EquiRectangularCubeMap::convertToCubeMap(int texture_unit,
                                                  const glm::mat4 &projectionMatrix,
                                                  const glm::mat4 captureViews[6])
    {
        if (!shader || !cubeMap || !cubeMapBuffer)
        {
            std::cerr << "EquiRectangularCubeMap: Missing shader, cubeMap, or cubeMapBuffer\n";
            return;
        }

        shader->use();
        shader->set("equirectangularMap", texture_unit);
        shader->set("projection", projectionMatrix);

        glViewport(0, 0, cubeMapBuffer->size.x, cubeMapBuffer->size.y);
        cubeMapBuffer->bindFrameBuffer();
        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "EquiRectangularCubeMap: Framebuffer not complete!" << std::endl;
            cubeMapBuffer->unbindFrameBuffer();
            return;
        }

        for (unsigned int i = 0; i < 6; ++i)
        {
            shader->set("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMap->name, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            our::mesh_utils::renderCube();
        }

        cubeMapBuffer->unbindFrameBuffer();
    }

    void IrradianceCubeMap::convertToCubeMap(int texture_unit,
                                             const glm::mat4 &projectionMatrix,
                                             const glm::mat4 captureViews[6])
    {
        if (!shader || !cubeMap || !cubeMapBuffer)
        {
            std::cerr << "IrradianceCubeMap: Missing shader, cubeMap, or cubeMapBuffer\n";
            return;
        }

        shader->use();
        shader->set("environmentMap", texture_unit);
        shader->set("projection", projectionMatrix);

        glViewport(0, 0, cubeMapBuffer->size.x, cubeMapBuffer->size.y);
        cubeMapBuffer->bindFrameBuffer();
        // Check framebuffer completeness
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            std::cerr << "IrradianceCubeMap: Framebuffer not complete!" << std::endl;
            cubeMapBuffer->unbindFrameBuffer();
            return;
        }

        for (unsigned int i = 0; i < 6; ++i)
        {
            shader->set("view", captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMap->name, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            our::mesh_utils::renderCube();
        }

        cubeMapBuffer->unbindFrameBuffer();
    }

    void PrefilterCubeMap::convertToCubeMap(int texture_unit,
                                            const glm::mat4 &projectionMatrix,
                                            const glm::mat4 captureViews[6],
                                            GLuint maxMipLevels)
    {
        if (!shader || !cubeMap || !cubeMapBuffer)
        {
            std::cerr << "PrefilterCubeMap: Missing shader, cubeMap, or cubeMapBuffer\n";
            return;
        }

        shader->use();
        shader->set("environmentMap", texture_unit);
        shader->set("projection", projectionMatrix);

        cubeMapBuffer->bindFrameBuffer();

        // Save the original size as base dimensions.
        const unsigned int baseWidth = cubeMapBuffer->size.x;
        const unsigned int baseHeight = cubeMapBuffer->size.y;

        for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
        {
            // Calculate the dimensions for the current mip level using the base size.
            unsigned int mipWidth = static_cast<unsigned int>(baseWidth * std::pow(0.5f, mip));
            unsigned int mipHeight = static_cast<unsigned int>(baseHeight * std::pow(0.5f, mip));

            cubeMapBuffer->bindRenderBuffer();
            cubeMapBuffer->setRenderBufferStorage(GL_DEPTH_COMPONENT24, mipWidth, mipHeight);

            glViewport(0, 0, mipWidth, mipHeight);

            float roughness = (float)mip / (maxMipLevels - 1);
            shader->set("roughness", roughness);

            for (unsigned int i = 0; i < 6; ++i)
            {
                shader->set("view", captureViews[i]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, cubeMap->name, mip);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                our::mesh_utils::renderCube();
            }
        }

        cubeMapBuffer->unbindFrameBuffer();
    }

}
