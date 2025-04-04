#pragma once

#include <glm/glm.hpp>
#include <json/json.hpp>
// We will support 3 types of lights.
// 1- Directional Light: where we assume that the light rays are parallel. We use this to approximate sun light.
// 2- Point Light: where we assume that the light source is a single point that emits light in every direction. It can be used to approximate light bulbs.
// 3- Spot Light: where we assume that the light source is a single point that emits light in the direction of a cone. It can be used to approximate torches, highway light poles.
namespace our {
    enum class LightType {
        DIRECTIONAL,
        POINT,
        SPOT
    };

    struct Light {
        public:
            // Here we define our light. First member specifies its type.
            LightType type;
            // We also define the color & intensity of the light for each component of the Phong model (Ambient, Diffuse, Specular).
            glm::vec3 diffuse, specular, ambient;
            // we can use light instead of 3 components for more realistic lighting.
            // This is a bit more realistic since light color shouldn't differ between diffuse and specular.
            // But you may want to keep them separate if you want extra artistic control where you may want to ignore realism.
            glm::vec3 color; 
            glm::vec3 position; // Used for Point and Spot Lights only
            glm::vec3 direction; // Used for Directional and Spot Lights only
            float shininess; // specular power defines the smoothness/roughness
            bool realistic; // To choose weather to use the realistic lighting model or the old one.
            bool enabled; // Whether the light is enabled or not. If false, the light will not affect the scene.
            // This affects how the light will dim out as we go further from the light.
            // The formula is light_received = light_emitted / (a*d^2 + b*d + c) where a, b, c are the quadratic, linear and constant factors respectively.
            struct {
                float constant, linear, quadratic;
            } attenuation; // Used for Point and Spot Lights only
            // This specifies the inner and outer cone of the spot light.
            // The light power is 0 outside the outer cone, the light power is full inside the inner cone.
            // The light power is interpolated in between the inner and outer cone.
            struct {
                float inner, outer;
            } spot_angle; // Used for Spot Lights only

            // Deserializes the entity data and components from a json object
            void deserialize(const nlohmann::json&);
    };
}