#pragma once


#include <glad/gl.h>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <json/json.hpp>

namespace our {

    class CubeMapBuffer {
    public:
        // The OpenGL object name of the frame buffer used to render to this cube map
        GLuint framebuffer = 0;
        // The OpenGL object name of the render buffer used to render to this cube map
        GLuint renderbuffer = 0;
        // The size of the cube map faces
        glm::ivec2 size = { 512 , 512 };

        void setupFrameBuffer();

        void setupRenderBuffer();

        void setRenderBufferStorage(GLenum format, int width, int height) {
            glRenderbufferStorage(GL_RENDERBUFFER, format, width, height);
        }

        void bindFrameBuffer() {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        }

        void bindRenderBuffer() {
            glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        }

        void unbindFrameBuffer() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
        void unbindRenderBuffer() {
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        void bind() {
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        }

        void unbind() {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }
    };

  

}