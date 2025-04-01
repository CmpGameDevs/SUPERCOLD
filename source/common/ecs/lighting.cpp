#include "entity.hpp"
#include "lighting.hpp"
#include "../deserialize-utils.hpp"
#include <iostream>
#include <glm/gtx/euler_angles.hpp>

namespace our {

     // Deserializes the entity data and components from a json object
    void Light::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        // Read the light type
        if(data.contains("type")){
            std::string type = data.value("type", "");
            if(type == "directional"){
                this->type = LightType::DIRECTIONAL;
            }else if(type == "point"){
                this->type = LightType::POINT;
            }else if(type == "spot"){
                this->type = LightType::SPOT;
            }else{
                std::cerr << "Unknown light type: " << type << std::endl;
                return;
            }
        }
        // Read the light properties
        enabled = data.value("enabled", true);
        diffuse = data.value("diffuse", glm::vec3{0.0f, 0.0f, 0.0f});
        specular = data.value("specular", glm::vec3{0.0f, 0.0f, 0.0f});
        ambient = data.value("ambient", glm::vec3{0.0f, 0.0f, 0.0f});
        shininess = data.value("shininess", 1.0f);
        position = data.value("position", glm::vec3{0.0f, 0.0f, 0.0f});
        direction = data.value("direction", glm::vec3{0.0f, 0.0f, 0.0f});
        attenuation.constant = data.value("attenuation.constant", 1.0f);
        attenuation.linear = data.value("attenuation.linear", 0.0f);
        attenuation.quadratic = data.value("attenuation.quadratic", 0.0f);
        spot_angle.inner = glm::radians(data.value("spot_angle.inner", 12.5f));
        spot_angle.outer = glm::radians(data.value("spot_angle.outer", 15.0f));
    }

}