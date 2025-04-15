#pragma once


#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <json/json.hpp>

namespace our {

    class CubeMapTexture {
    public:
        // The OpenGL object name of this cube map
        GLuint name = 0;

        //? GL_RGB16F : 16-bit floating point RGB format, good for HDR images
        //? GL_RGBA16F : 16-bit floating point RGBA format, good for HDR images with alpha channel
        //? GL_RGB : 8-bit unsigned integer RGB format, good for standard images
        void setupCubeTexture(glm::ivec2 size, GLenum internal_format = GL_RGB16F ,GLenum format = GL_RGB, GLenum type = GL_FLOAT ,bool generate_mipmap = false);

        void generateMipmaps();

        void bind() const { 
            glBindTexture(GL_TEXTURE_CUBE_MAP, name); 
        }

        void unbind() const { 
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0); 
        }
       
    };

  

}