#include "texture-utils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <glm/glm.hpp>
#include <iostream>

our::Texture2D* our::texture_utils::empty(GLenum format, glm::ivec2 size) {
    our::Texture2D* texture = new our::Texture2D();
    // TODO: (Req 11) Finish this function to create an empty texture with the given size and format
    GLuint levelsCnt = glm::floor(glm::log2(glm::max<float>(size.x, size.y))) + 1;
    glTexStorage2D(GL_TEXTURE_2D, levelsCnt, format, size.x, size.y);
    texture->bind();

    return texture;
}

our::Texture2D* our::texture_utils::loadImage(const std::string& filename, bool generate_mipmap) {
    glm::ivec2 size;
    int channels;
    // Since OpenGL puts the texture origin at the bottom left while images typically has the origin at the top left,
    // We need to till stb to flip images vertically after loading them
    stbi_set_flip_vertically_on_load(true);
    // Load image data and retrieve width, height and number of channels in the image
    // The last argument is the number of channels we want and it can have the following values:
    //- 0: Keep number of channels the same as in the image file
    //- 1: Grayscale only
    //- 2: Grayscale and Alpha
    //- 3: RGB
    //- 4: RGB and Alpha (RGBA)
    // Note: channels (the 4th argument) always returns the original number of channels in the file
    unsigned char* pixels = stbi_load(filename.c_str(), &size.x, &size.y, &channels, 4);
    if (pixels == nullptr) {
        std::cerr << "Failed to load image: " << filename << std::endl;
        return nullptr;
    }
    // Create a texture
    our::Texture2D* texture = new our::Texture2D();
    // Bind the texture such that we upload the image data to its storage
    // TODO: (Req 5) Finish this function to fill the texture with the data found in "pixels"
    texture->bind();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    if (generate_mipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    stbi_image_free(pixels); // Free image data after uploading to GPU
    return texture;
}

our::Texture2D* our::texture_utils::loadHDR(const std::string& filename, bool generate_mipmap) {
    glm::ivec2 size;
    int channels;
    // Since OpenGL puts the texture origin at the bottom left while images typically has the origin at the top left,
    // We need to till stb to flip images vertically after loading them
    stbi_set_flip_vertically_on_load(true);
    float* pixels = stbi_loadf(filename.c_str(), &size.x, &size.y, &channels, 0);
    if (pixels == nullptr) {
        std::cerr << "Failed to load HDR: " << filename << std::endl;
        return nullptr;
    }
    // Create a texture
    our::Texture2D* texture = new our::Texture2D();
    texture->bind();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, size.x, size.y, 0, GL_RGB, GL_FLOAT, pixels);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (generate_mipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    stbi_image_free(pixels); // Free image data after uploading to GPU
    return texture;
}

our::Texture2D* our::texture_utils::loadFromMemory(const unsigned char* data, int size, bool generate_mipmap) {
    if (!data || size <= 0) {
        std::cerr << "Invalid data or size for embedded texture" << std::endl;
        return nullptr;
    }

    glm::ivec2 imageSize;
    int channels;

    // Since OpenGL puts the texture origin at the bottom left while images typically has the origin at the top left,
    // We need to tell stb to flip images vertically after loading them
    stbi_set_flip_vertically_on_load(true);

    // Load image data from memory buffer
    unsigned char* pixels = stbi_load_from_memory(data, size, &imageSize.x, &imageSize.y, &channels, 4);
    if (pixels == nullptr) {
        std::cerr << "Failed to load embedded texture from memory buffer" << std::endl;
        return nullptr;
    }

    // Create a texture
    our::Texture2D* texture = new our::Texture2D();
    texture->bind();
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, imageSize.x, imageSize.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    if (generate_mipmap) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    stbi_image_free(pixels); // Free image data after uploading to GPU
    return texture;
}
