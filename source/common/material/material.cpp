#include "material.hpp"

#include "../asset-loader.hpp"
#include "deserialize-utils.hpp"
#include <iostream>
namespace our
{

    // This function should setup the pipeline state and set the shader to be used
    void Material::setup() const
    {
        // TODO: (Req 7) Write this function
        pipelineState.setup();
        shader->use();
    }

    // This function read the material data from a json object
    void Material::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;

        if (data.contains("pipelineState"))
        {
            pipelineState.deserialize(data["pipelineState"]);
        }
        shader = AssetLoader<ShaderProgram>::get(data["shader"].get<std::string>());
        transparent = data.value("transparent", false);
    }

    // This function should call the setup of its parent and
    // set the "tint" uniform to the value in the member variable tint
    void TintedMaterial::setup() const
    {
        // TODO: (Req 7) Write this function
        Material::setup();
        shader->set("tint", tint);
    }

    // This function read the material data from a json object
    void TintedMaterial::deserialize(const nlohmann::json &data)
    {
        Material::deserialize(data);
        if (!data.is_object())
            return;
        tint = data.value("tint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // This function should call the setup of its parent and
    // set the "alphaThreshold" uniform to the value in the member variable alphaThreshold
    // Then it should bind the texture and sampler to a texture unit and send the unit number to the uniform variable "tex"
    void TexturedMaterial::setup() const
    {
        // TODO: (Req 7) Write this function
        TintedMaterial::setup();
        shader->set("alphaThreshold", alphaThreshold);
        glActiveTexture(GL_TEXTURE0);
        texture->bind();
        sampler->bind(0);
        shader->set("tex", 0);
    }

    // This function read the material data from a json object
    void TexturedMaterial::deserialize(const nlohmann::json &data)
    {
        TintedMaterial::deserialize(data);
        if (!data.is_object())
            return;
        alphaThreshold = data.value("alphaThreshold", 0.0f);
        texture = AssetLoader<Texture2D>::get(data.value("texture", ""));
        sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
    }

    void LitMaterial::setup() const
    {
        TexturedMaterial::setup();
        shader->set("material.useTextureAlbedo", useTextureAlbedo);
        shader->set("material.useTextureMetallic", useTextureMetallic);
        shader->set("material.useTextureRoughness", useTextureRoughness);
        shader->set("material.useTextureNormal", useTextureNormal);
        shader->set("material.useTextureAmbientOcclusion", useTextureAmbientOcclusion);
        shader->set("material.useTextureEmissive", useTextureEmissive);
        shader->set("material.albedo", albedo);
        shader->set("material.metallic", metallic);
        shader->set("material.roughness", roughness);
        shader->set("material.ambientOcclusion", ambientOcclusion);
        shader->set("material.emission", emission);
        if (useTextureAlbedo)
        {
            glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ALBEDO);
            textureAlbedo->bind();
            shader->set("material.textureAlbedo", TEXTURE_UNIT_ALBEDO);
        }
        if (useTextureMetallic)
        {
            glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_METALLIC);
            textureMetallic->bind();
            shader->set("material.textureMetallic", TEXTURE_UNIT_METALLIC);
        }
        if (useTextureRoughness)
        {
            glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_ROUGHNESS);
            textureRoughness->bind();
            shader->set("material.textureRoughness", TEXTURE_UNIT_ROUGHNESS);
        }
        if (useTextureNormal)
        {
            glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_NORMAL);
            textureNormal->bind();
            shader->set("material.textureNormal", TEXTURE_UNIT_NORMAL);
        }
        if (useTextureAmbientOcclusion)
        {
            glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_AMBIENT_OCCLUSION);
            textureAmbientOcclusion->bind();
            shader->set("material.textureAmbientOcclusion", TEXTURE_UNIT_AMBIENT_OCCLUSION);
        }
        if (useTextureEmissive)
        {
            glActiveTexture(GL_TEXTURE0 + TEXTURE_UNIT_EMISSIVE);
            textureEmissive->bind();
            shader->set("material.textureEmissive", TEXTURE_UNIT_EMISSIVE);
        }
    }

    void LitMaterial::deserialize(const nlohmann::json &data)
    {
        TexturedMaterial::deserialize(data);
        if (!data.is_object())
            return;
        useTextureAlbedo = data.value("useTextureAlbedo", false);
        useTextureMetallic = data.value("useTextureMetallic", false);
        useTextureRoughness = data.value("useTextureRoughness", false);
        useTextureNormal = data.value("useTextureNormal", false);
        useTextureAmbientOcclusion = data.value("useTextureAmbientOcclusion", false);
        useTextureEmissive = data.value("useTextureEmissive", false);
        albedo = data.value("albedo", glm::vec3(1.0f, 1.0f, 1.0f));
        metallic = data.value("metallic", 1.0f);
        roughness = data.value("roughness", 0.0f);
        ambientOcclusion = data.value("ambientOcclusion", 1.0f);
        emission = data.value("emission", glm::vec3(0.0f, 0.0f, 0.0f));
        textureAlbedo = AssetLoader<Texture2D>::get(data.value("textureAlbedo", ""));
        textureMetallic = AssetLoader<Texture2D>::get(data.value("textureMetallic", ""));
        textureRoughness = AssetLoader<Texture2D>::get(data.value("textureRoughness", ""));
        textureNormal = AssetLoader<Texture2D>::get(data.value("textureNormal", ""));
        textureAmbientOcclusion = AssetLoader<Texture2D>::get(data.value("textureAmbientOcclusion", ""));
        textureEmissive = AssetLoader<Texture2D>::get(data.value("textureEmissive", ""));
    }

}