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

    void TintedMaterial::teardown() {
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
        // sampler->bind(0);
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
        TintedMaterial::setup();

        shader->set("material.useTextureAlbedo", useTextureAlbedo);
        shader->set("material.useTextureMetallic", useTextureMetallic);
        shader->set("material.useTextureRoughness", useTextureRoughness);
        shader->set("material.useTextureMetallicRoughness", useTextureMetallicRoughness);
        shader->set("material.useTextureNormal", useTextureNormal);
        shader->set("material.useTextureAmbientOcclusion", useTextureAmbientOcclusion);
        shader->set("material.useTextureEmissive", useTextureEmissive);
        shader->set("material.albedo", albedo);
        shader->set("material.metallic", metallic);
        shader->set("material.roughness", roughness);
        shader->set("material.ambientOcclusion", ambientOcclusion);
        shader->set("material.emission", emission);
        shader->set("irradianceMap", our::TextureUnits::TEXTURE_UNIT_IRRADIANCE);
        shader->set("prefilterMap", our::TextureUnits::TEXTURE_UNIT_PREFILTER);
        shader->set("brdfLUT", our::TextureUnits::TEXTURE_UNIT_BRDF);
        shader->set("lightCount", static_cast<int>(lights.size()));
        setupLight();
        if (useTextureAlbedo)
        {
            glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_ALBEDO);
            textureAlbedo->bind();
            shader->set("material.textureAlbedo", our::TextureUnits::TEXTURE_UNIT_ALBEDO);
        }
        if (useTextureMetallic)
        {
            glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_METALLIC);
            textureMetallic->bind();
            shader->set("material.textureMetallic", our::TextureUnits::TEXTURE_UNIT_METALLIC);
        }
        if (useTextureRoughness)
        {
            glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_ROUGHNESS);
            textureRoughness->bind();
            shader->set("material.textureRoughness", our::TextureUnits::TEXTURE_UNIT_ROUGHNESS);
        }
        if (useTextureMetallicRoughness)
        {
            glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_METALLIC_ROUGHNESS);
            textureMetallicRoughness->bind();
            shader->set("material.textureMetallicRoughness", our::TextureUnits::TEXTURE_UNIT_METALLIC_ROUGHNESS);
        }
        if (useTextureNormal)
        {
            glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_NORMAL);
            textureNormal->bind();
            shader->set("material.textureNormal", our::TextureUnits::TEXTURE_UNIT_NORMAL);
        }
        if (useTextureAmbientOcclusion)
        {
            glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_AMBIENT_OCCLUSION);
            textureAmbientOcclusion->bind();
            shader->set("material.textureAmbientOcclusion", our::TextureUnits::TEXTURE_UNIT_AMBIENT_OCCLUSION);
        }
        if (useTextureEmissive)
        {
            glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_EMISSIVE);
            textureEmissive->bind();
            shader->set("material.textureEmissive", our::TextureUnits::TEXTURE_UNIT_EMISSIVE);
        }
    }

    void LitMaterial::setupLight() const
    {
        int light_index = 0;
        for (const auto &light : lights)
        {
            if (light->enabled)
            {
                std::string prefix = "lights[" + std::to_string(light_index) + "].";

                shader->set(prefix + "color", light->color);
                shader->set(prefix + "type", int(light->type));
     
                switch (light->type)
                {
                case our::LightType::DIRECTIONAL:
                    shader->set(prefix + "direction", normalize(light->direction));
                    break;
                case our::LightType::POINT:
                    shader->set(prefix + "position", light->position);
                    shader->set(prefix + "attenuation_constant", light->attenuation.constant);
                    shader->set(prefix + "attenuation_linear", light->attenuation.linear);
                    shader->set(prefix + "attenuation_quadratic", light->attenuation.quadratic);
                    break;
                case our::LightType::SPOT:
                    shader->set(prefix + "position", light->position);
                    shader->set(prefix + "direction", glm::normalize(light->direction));
                    shader->set(prefix + "attenuation_constant", light->attenuation.constant);
                    shader->set(prefix + "attenuation_linear", light->attenuation.linear);
                    shader->set(prefix + "attenuation_quadratic", light->attenuation.quadratic);
                    shader->set(prefix + "inner_angle", light->spot_angle.inner);
                    shader->set(prefix + "outer_angle", light->spot_angle.outer);
                    break;
                }
            }
            light_index++;
        }
    }

    void LitMaterial::deserialize(const nlohmann::json &data)
    {
        TintedMaterial::deserialize(data);
        if (!data.is_object())
            return;
        useTextureAlbedo = data.value("useTextureAlbedo", false);
        useTextureMetallic = data.value("useTextureMetallic", false);
        useTextureRoughness = data.value("useTextureRoughness", false);
        useTextureMetallicRoughness = data.value("useTextureMetallicRoughness", false);
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
        textureMetallicRoughness = AssetLoader<Texture2D>::get(data.value("textureMetallicRoughness", ""));
        textureNormal = AssetLoader<Texture2D>::get(data.value("textureNormal", ""));
        textureAmbientOcclusion = AssetLoader<Texture2D>::get(data.value("textureAmbientOcclusion", ""));
        textureEmissive = AssetLoader<Texture2D>::get(data.value("textureEmissive", ""));

        if (data.contains("lights"))
        {
            if (data["lights"].is_array())
            {
                lights.clear();
                auto lightNames = data["lights"].get<std::vector<std::string>>();
                for (const auto &name : lightNames)
                {
                    auto lightPtr = AssetLoader<Light>::get(name);
                    if (lightPtr)
                    {
                        lights.push_back(lightPtr);
                    }
                }
            }
        }
    }

}