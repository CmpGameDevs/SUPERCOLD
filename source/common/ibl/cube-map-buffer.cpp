#include "cube-map-buffer.hpp"

#include "../asset-loader.hpp"
#include "deserialize-utils.hpp"
#include <iostream>
namespace our
{

   void CubeMapBuffer::setupFrameBuffer()
   {
      glGenFramebuffers(1, &framebuffer);
      glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
      GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
      if (status != GL_FRAMEBUFFER_COMPLETE)
         std::cerr << "Error: Framebuffer is not complete!" << std::endl;
   }

    void CubeMapBuffer::setupRenderBuffer()
    {
        glGenRenderbuffers(1, &renderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
    }



}







