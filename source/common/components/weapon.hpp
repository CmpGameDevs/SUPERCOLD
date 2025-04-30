#pragma once

#include <glm/glm.hpp>
#include <ecs/component.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

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
        float bulletSize = 0.2f;
        glm::quat weaponRotation = glm::identity<glm::quat>();

        static std::string getID() { return "Weapon"; }

        void deserialize(const nlohmann::json& data) override;
    };
}