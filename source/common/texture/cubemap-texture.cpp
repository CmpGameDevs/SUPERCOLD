#include "cubemap-texture.hpp"

#include "../asset-loader.hpp"
#include "deserialize-utils.hpp"
#include <iostream>
namespace our
{

    void our::CubeMapTexture::setupCubeTexture(glm::ivec2 size, GLenum internal_format ,GLenum format, GLenum type, bool generate_mipmap){
        glGenTextures(1, &name);
        glBindTexture(GL_TEXTURE_CUBE_MAP, name);
        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format, size.x, size.y, 0, format, type, nullptr);
        }

        // ? Linear filtering → smooth when sampling (no pixelated edges)
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        // ? Clamp to edge prevents visible seams at the cubemap edges when sampling
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        if (generate_mipmap) {
            // ? Better for PBR
            // ? Mipmaps are used in PBR to simulate roughness: rougher surfaces reflect lower mip levels (blurry), while smooth ones reflect higher ones (sharp).
            glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        }
    }

    void our::CubeMapTexture::generateMipmaps() {
        glBindTexture(GL_TEXTURE_CUBE_MAP, name);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
    }



}







