#pragma once

#include <glm/glm.hpp>
#include <ecs/component.hpp>

namespace our {

    class WeaponComponent : public Component {
    public:
        float damage = 0.0f;
        float range = 0.0f;
        float fireRate = 0.0f;
        float fireCooldown = 0.0f;
        float reloadTime = 0.0f;
        int ammoCapacity = 0;
        int currentAmmo = 0;
        bool automatic = false;
        float throwForce = 1.0f;

        static std::string getID() { return "Weapon"; }

        void deserialize(const nlohmann::json& data) override;
    };
}