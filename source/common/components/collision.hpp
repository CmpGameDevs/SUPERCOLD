#pragma once

#include <unordered_set>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <glm/glm.hpp>
#include "../ecs/component.hpp"
#include "../mesh/vertex.hpp"

namespace our {

    // Collision shape types supported by our collision system
    enum class CollisionShape {
        BOX,
        SPHERE,
        CAPSULE,
        MESH,
        GHOST
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

        // For mesh collision, we need to store the vertices and indices of the mesh
        std::vector<our::Vertex> vertices;
        std::vector<uint32_t> indices;
        btTriangleMesh* triangleMesh = nullptr;

        // For ghost collision, we need to store the ghost object
        btPairCachingGhostObject* ghostObject = nullptr;

        // For collision detection, we need to store the collided entities
        std::unordered_set<Entity*> collidedEntities;

        // For realism add drag and cross-section
        float dragCoefficient = 1.0f;
        float crossSectionArea = 1.0f;

        virtual ~CollisionComponent();

        void freeBulletBody();

        // The ID of this component type is "Collision"
        static std::string getID() { return "Collision"; }

        // Reads collision properties from the given json object
        void deserialize(const nlohmann::json& data) override;
    };

}