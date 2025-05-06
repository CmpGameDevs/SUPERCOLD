#pragma once

#include <glm/glm.hpp>
#include <ecs/component.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <model/model.hpp>
#include <asset-loader.hpp>

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
        float muzzleForwardOffset = 0.15f; 
        float muzzleRightOffset = -0.4f;  
        glm::vec3 weaponPosition = glm::vec3(0.6f, -0.2f, -0.4f);
        glm::quat weaponRotation = glm::identity<glm::quat>();
        glm::quat bulletRotation = glm::identity<glm::quat>();
        glm::vec3 bulletScale = glm::vec3(1.0f, 1.0f, 1.0f);
        Model* model = nullptr;

        static std::string getID() { return "Weapon"; }

        void deserialize(const nlohmann::json& data) override;
    };
}