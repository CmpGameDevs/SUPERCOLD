#pragma once

#include <iostream>
#include <shader/shader.hpp>
#include <texture/cubemap-texture.hpp>
#include <ibl/cubemap-buffer.hpp>
#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <json/json.hpp>
#include <mesh/mesh-utils.hpp>

namespace our
{

    class CubeMap
    {
    protected:
        ShaderProgram *shader = nullptr;
        CubeMapTexture *cubeMap = nullptr;
        CubeMapBuffer *cubeMapBuffer = nullptr;

    public:
        CubeMap(ShaderProgram *shader,
                CubeMapTexture *cubeMap,
                CubeMapBuffer *cubeMapBuffer)
            : shader(shader), cubeMap(cubeMap), cubeMapBuffer(cubeMapBuffer) {}

        virtual ~CubeMap() = default;

        virtual void convertToCubeMap(int texture_unit,
                                      const glm::mat4 &projectionMatrix,
                                      const glm::mat4 captureViews[6]) = 0;
    };

    class EquiRectangularCubeMap : public CubeMap
    {
    public:
        EquiRectangularCubeMap(ShaderProgram *shader,
                               CubeMapTexture *cubeMap,
                               CubeMapBuffer *cubeMapBuffer)
            : CubeMap(shader, cubeMap, cubeMapBuffer) {}

        void convertToCubeMap(int texture_unit, const glm::mat4 &projectionMatrix, const glm::mat4 captureViews[6]) override;
    };

    class IrradianceCubeMap : public CubeMap
    {
    public:
        IrradianceCubeMap(ShaderProgram *shader,
                          CubeMapTexture *cubeMap,
                          CubeMapBuffer *cubeMapBuffer)
            : CubeMap(shader, cubeMap, cubeMapBuffer) {}

        void convertToCubeMap(int texture_unit,
                              const glm::mat4 &projectionMatrix,
                              const glm::mat4 captureViews[6]) override;
    };

    class PrefilterCubeMap : public CubeMap
    {
    public:
        GLuint maxMipLevels = 5;

        PrefilterCubeMap(ShaderProgram *shader,
                         CubeMapTexture *cubeMap,
                         CubeMapBuffer *cubeMapBuffer, 
                         GLuint maxMipLevels = 5)
            : CubeMap(shader, cubeMap, cubeMapBuffer) {
                this->maxMipLevels = maxMipLevels;
            }

        void convertToCubeMap(int texture_unit,
                              const glm::mat4 &projectionMatrix,
                              const glm::mat4 captureViews[6],
                              GLuint maxMipLevels = 5);

        // Override the base method with default mip level
        void convertToCubeMap(int texture_unit,
                              const glm::mat4 &projectionMatrix,
                              const glm::mat4 captureViews[6]) override
        {
            convertToCubeMap(texture_unit, projectionMatrix, captureViews, maxMipLevels);
        }
    };

}
