#pragma once

#include <map>
#include <string>
#include <application.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <systems/audio-system.hpp>
#include <glad/gl.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <shader/shader.hpp>
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <queue>
#include <tuple>

namespace our {

struct Character {
    GLuint textureID;
    glm::ivec2 size;
    glm::ivec2 bearing;
    GLuint advance;
};

class TextRenderer {
    std::map<char, Character> characters;
    GLuint VAO, VBO;
    ShaderProgram* textShader;
    glm::mat4 projection;
    float screenWidth, screenHeight;

    float fadeStartTime = -1.0f;
    float fadeDuration = 1.0f;
    float visibleDuration = 1.0f;
    float fadeOutDuration = 0.25f;
    std::string fadeText = "";
    float fadeScale = 1.0f;
    glm::vec3 fadeColor = glm::vec3(1.0f);
    bool active = false;

    std::queue<std::tuple<std::string, float, float, float, glm::vec3>> textQueue;
    bool isQueueActive = false;

public:
    static TextRenderer& getInstance() {
        static TextRenderer instance;
        return instance;
    }

    void initialize(float w, float h) {
        std::string fontPath = "assets/font/font2.ttf";
        textShader = new our::ShaderProgram();
        textShader->attach("assets/shaders/text.vert", GL_VERTEX_SHADER);
        textShader->attach("assets/shaders/text.frag", GL_FRAGMENT_SHADER);
        textShader->link();
        textShader->use();
        screenWidth = w;
        screenHeight = h;
        projection = glm::ortho(0.0f, screenWidth, 0.0f, screenHeight);
        textShader->use();
        textShader->set("projection", projection);
        textShader->set("text", 0);

        FT_Library ft;
        if (FT_Init_FreeType(&ft)) throw std::runtime_error("Could not init FreeType");

        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) throw std::runtime_error("Failed to load font: " + fontPath);

        FT_Set_Pixel_Sizes(face, 0, 2024);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        for (unsigned char c = 0; c < 128; c++) {
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;

            GLuint tex;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            characters[c] = {
                tex,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                static_cast<GLuint>(face->glyph->advance.x)
            };
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        FT_Done_Face(face);
        FT_Done_FreeType(ft);

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    glm::vec2 getTextSize(const std::string& text, float scale) {
        float width = 0.0f, height = 0.0f;
        for (char c : text) {
            Character ch = characters[c];
            width += (ch.advance >> 6) * scale;
            float h = ch.size.y * scale;
            if (h > height) height = h;
        }
        return glm::vec2(width, height);
    }

    void renderText(const std::string& text, float x, float y, float scale, glm::vec4 color) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        textShader->use();
        textShader->set("textColor", color);
        textShader->set("projection", projection);
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        for (char c : text) {
            Character ch = characters[c];
            float xpos = x + ch.bearing.x * scale;
            float ypos = y - (ch.size.y - ch.bearing.y) * scale;
            float w = ch.size.x * scale;
            float h = ch.size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }
            };

            glBindTexture(GL_TEXTURE_2D, ch.textureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            x += (ch.advance >> 6) * scale;
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    void showCenteredText(const std::string& text, float scale = 0.1f, glm::vec3 color = glm::vec3(1.0f),
                          float fadeIn = 0.05f, float visible = 1.0f, float fadeOut = 0.005f) {
        textQueue.push({ text, fadeIn, visible, fadeOut, color });

        if (!isQueueActive) {
            isQueueActive = true;
            showNextText();  
        }
    }

    void showNextText() {
        if (textQueue.empty()) {
            isQueueActive = false; 
            return;
        }

        auto [text, fadeIn, visible, fadeOut, color] = textQueue.front();
        textQueue.pop();
        
        AudioSystem::getInstance().playSfx("shutter", false, 3.0f);
        fadeStartTime = static_cast<float>(glfwGetTime());
        fadeDuration = fadeIn;
        visibleDuration = visible;
        fadeOutDuration = fadeOut;
        fadeText = text;
        fadeScale = 0.1f;
        fadeColor = color;
        active = true;
    }

    void renderCenteredText() {
        if (!active) return;

        float currentTime = static_cast<float>(glfwGetTime());
        float elapsed = currentTime - fadeStartTime;
        float totalDuration = fadeDuration + visibleDuration + fadeOutDuration;

        if (elapsed > totalDuration) {
            active = false;
            showNextText();
            return;
        }

        float alpha = 1.0f;

        if (elapsed < fadeDuration) {
            alpha = elapsed / fadeDuration;
        }
        else if (elapsed < fadeDuration + visibleDuration) {
            alpha = 1.0f;
        }
        else {
            float fadeOutTime = elapsed - fadeDuration - visibleDuration;
            alpha = 1.0f - (fadeOutTime / fadeOutDuration);
            if (alpha < 0.0f) alpha = 0.0f;
        }

        glm::vec2 size = getTextSize(fadeText, fadeScale);
        float x = (screenWidth - size.x) / 2.0f;
        float y = (screenHeight - size.y) / 2.0f;
        renderText(fadeText, x, y, fadeScale, glm::vec4(fadeColor, alpha));
    }

    void destroy() {
        for (auto& [_, ch] : characters) {
            glDeleteTextures(1, &ch.textureID);
        }
        glDeleteBuffers(1, &VBO);
        glDeleteVertexArrays(1, &VAO);
        delete textShader;
        textShader = nullptr;
    }
};

}
