#include "weapon.hpp"
#include "../deserialize-utils.hpp"
#include <glm/gtc/quaternion.hpp> 
#include <glm/gtx/euler_angles.hpp>

namespace our {
    // Reads linearVelocity & angularVelocity from the given json object
    void WeaponComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;
        damage = data.value("damage", damage);
        range = data.value("range", range);
        fireRate = data.value("fireRate", fireRate);
        reloadTime = data.value("reloadTime", reloadTime);
        ammoCapacity = data.value("ammoCapacity", ammoCapacity);
        currentAmmo = data.value("currentAmmo", ammoCapacity);
        automatic = data.value("automatic", automatic);
        throwForce = data.value("throwForce", throwForce);
        bulletSize = data.value("bulletSize", bulletSize);
        glm::vec3 currentEuler = glm::degrees(glm::eulerAngles(weaponRotation));
        glm::vec3 eulerDegrees = data.value("rotation", currentEuler);
        glm::vec3 eulerRadians = glm::radians(eulerDegrees);
        glm::mat4 rotMat = glm::yawPitchRoll(eulerRadians.y, eulerRadians.x, eulerRadians.z);
        weaponRotation = glm::quat_cast(rotMat);
    }
}