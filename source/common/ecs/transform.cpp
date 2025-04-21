#include "entity.hpp"
#include "../deserialize-utils.hpp"
#include <glm/gtc/quaternion.hpp> 
#include <glm/gtx/euler_angles.hpp>

namespace our {

    // This function computes and returns a matrix that represents this transform
    // Remember that the order of transformations is: Scaling, Rotation then Translation
    // HINT: to convert euler angles to a rotation matrix, you can use glm::yawPitchRoll
    glm::mat4 Transform::toMat4() const {
        //TODO: (Req 3) Write this function
        glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMatrix = glm::mat4_cast(rotation);
        glm::mat4 scalingMatrix = glm::scale(glm::mat4(1.0f), scale);
        
        return translationMatrix * rotationMatrix * scalingMatrix;
    }

     // Deserializes the entity data and components from a json object
    void Transform::deserialize(const nlohmann::json& data) {
        position = data.value("position", position);
        
        // Convert current rotation to Euler angles for default
        glm::vec3 currentEuler = glm::degrees(glm::eulerAngles(rotation));
        glm::vec3 eulerDegrees = data.value("rotation", currentEuler);
        glm::vec3 eulerRadians = glm::radians(eulerDegrees);
        
        // Create matrix from Euler angles (Yaw, Pitch, Roll)
        glm::mat4 rotMat = glm::yawPitchRoll(eulerRadians.y, eulerRadians.x, eulerRadians.z);
        rotation = glm::quat_cast(rotMat);
        
        scale = data.value("scale", scale);
    }

}