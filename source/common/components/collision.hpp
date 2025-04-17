// source/common/components/collision.hpp
#pragma once

#include "../ecs/component.hpp"
#include <btBulletDynamicsCommon.h>
#include <glm/glm.hpp>

namespace our {

    // Collision shape types supported by our collision system
    enum class CollisionShape {
        BOX,
        SPHERE,
        CAPSULE
    };

    // This component denotes that the CollisionSystem will check for collisions with this entity.
    // It stores the shape and size of the collision volume.
    // For more information, see "common/systems/collision.hpp"
    class CollisionComponent : public Component {
    public:
        float mass = 0.0f;
        CollisionShape shape = CollisionShape::BOX;
        glm::vec3 halfExtents {0.5f};
        bool isKinematic = false;
        btRigidBody* bulletBody = nullptr;

        // The ID of this component type is "Collision"
        static std::string getID() { return "Collision"; }

        // Reads collision properties from the given json object
        void deserialize(const nlohmann::json& data) override;
    };

}