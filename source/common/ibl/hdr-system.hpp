#pragma once

#include <iostream>
#include <shader/shader.hpp>
#include <texture/cubemap-texture.hpp>
#include <ibl/cubemap-buffer.hpp>
#include <ibl/cubemap.hpp>
#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <json/json.hpp>
#include <mesh/mesh-utils.hpp>
#include "asset-loader.hpp"
#include <texture/texture-utils.hpp>
#include <texture/texture-unit.hpp>

namespace our
{

    class HDRSystem
    {
    public:
        ShaderProgram *equirectangular_shader;
        ShaderProgram *irradiance_shader;
        ShaderProgram *background_shader;
        ShaderProgram *brdf_shader;
        ShaderProgram *prefilter_shader;
        CubeMapBuffer *cubeMapBuffer;
        CubeMapTexture *envCubeMap;
        CubeMapTexture *irradianceMap;
        CubeMapTexture *prefilterMap;
        CubeMap *irradianceCubeMap;
        CubeMap *prefilterCubeMap;
        CubeMap *equirectangularCubeMap;
        our::Texture2D *brdfLUTTexture;
        our::Texture2D *hdr_texture;
        GLuint maxMipLevels = 5;
        bool enable = true;
        glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        glm::mat4 captureViews[6] =
            {
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, -1.0f, 0.0f)),
                glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, -1.0f, 0.0f))};

        void initialize();

        void initializeShader();

        void initializeCubeMap();

        void initializeBuffer();

        void initializeTexture();

        void setup(glm::ivec2 windowSize);

        void bindTextures();

        void unbindTextures();

        void deserialize(const nlohmann::json &data)
        {
            if (!data.is_object())
                return;
            hdr_texture = AssetLoader<Texture2D>::get(data.value("hdr_texture", ""));
            enable = data.value("enable", true);
            maxMipLevels = data.value("maxMipLevels", 5);
        }

        void renderBackground(glm::mat4 projection, glm::mat4 view);

        ~HDRSystem()
        {
            delete cubeMapBuffer;
            delete envCubeMap;
            delete irradianceMap;
            delete prefilterMap;
            delete irradianceCubeMap;
            delete prefilterCubeMap;
            delete equirectangularCubeMap;
        }
    };

}
