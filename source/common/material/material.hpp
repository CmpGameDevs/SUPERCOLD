#pragma once

#include "pipeline-state.hpp"
#include "../texture/texture2d.hpp"
#include "../texture/sampler.hpp"
#include "../shader/shader.hpp"
#include <texture/texture-unit.hpp>
#include <ecs/lighting.hpp>

#include <glm/vec4.hpp>
#include <json/json.hpp>


namespace our {

    // This is the base class for all the materials
    // It contains the 3 essential components required by any material
    // 1- The pipeline state when drawing objects using this material
    // 2- The shader program used to draw objects using this material
    // 3- Whether this material is transparent or not
    // Materials that send uniforms to the shader should inherit from the is material and add the required uniforms
    class Material {
    public:
        PipelineState pipelineState;
        ShaderProgram* shader;
        bool transparent;
        
        // This function does 2 things: setup the pipeline state and set the shader program to be used
        virtual void setup() const;
        // This function read a material from a json object
        virtual void deserialize(const nlohmann::json& data);
    };

    // This material adds a uniform for a tint (a color that will be sent to the shader)
    // An example where this material can be used is when the whole object has only color which defined by tint
    class TintedMaterial : public Material {
    public:
        glm::vec4 tint;

        void teardown();
        void setup() const override;
        void deserialize(const nlohmann::json& data) override;
    };

    // This material adds two uniforms (besides the tint from Tinted Material)
    // The uniforms are:
    // - "tex" which is a Sampler2D. "texture" and "sampler" will be bound to it.
    // - "alphaThreshold" which defined the alpha limit below which the pixel should be discarded
    // An example where this material can be used is when the object has a texture
    class TexturedMaterial : public TintedMaterial {
    public:
        Texture2D* texture;
        Sampler* sampler;
        float alphaThreshold;

        void setup() const override;
        void deserialize(const nlohmann::json& data) override;
    };

    // LitMaterial: Supports full PBR-like lighting with multiple textures
    class LitMaterial : public TintedMaterial {
        private:
            void setupLight() const;     
        public:
            bool useTextureAlbedo = false;
            bool useTextureMetallic = false;
            bool useTextureRoughness = false;
            bool useTextureMetallicRoughness = false;
            bool useTextureNormal = false;
            bool useTextureAmbientOcclusion = false;
            bool useTextureEmissive = false;
        
            glm::vec3 albedo = glm::vec3(1.0, 1.0, 1.0);
            float metallic = 0.2f;
            float roughness = 0.2f;
            float ambientOcclusion = 1.0f;
            glm::vec3 emission = glm::vec3(0.0, 0.0, 0.0);
        
            Texture2D* textureAlbedo;
            Texture2D* textureMetallic;
            Texture2D* textureRoughness;
            Texture2D* textureMetallicRoughness;
            Texture2D* textureNormal;
            Texture2D* textureAmbientOcclusion;
            Texture2D* textureEmissive;

            std::vector<our::Light*> lights;

            void setup() const override;
            void deserialize(const nlohmann::json& data) override;
        };

    // This function returns a new material instance based on the given type
    inline Material* createMaterialFromType(const std::string& type){
        if(type == "tinted"){
            return new TintedMaterial();
        } else if(type == "textured"){
            return new TexturedMaterial();
        } else if(type == "lit"){
            return new LitMaterial();
        }else {
            return new Material();
        }
    }

}
