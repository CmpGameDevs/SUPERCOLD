#include "weapon.hpp"
#include "../deserialize-utils.hpp"

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
    }
}