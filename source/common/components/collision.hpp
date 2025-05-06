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
        GHOST,
        COMPOUND
    };

    // This component denotes that the CollisionSystem will check for collisions with this entity.
    // It stores the shape and size of the collision volume.
    // For more information, see "common/systems/collision.hpp"
    class CollisionComponent : public Component {
    public:
        struct ChildShape {
            CollisionShape shape = CollisionShape::BOX;
            glm::vec3 halfExtents{0.5f};
            std::vector<our::Vertex> vertices;
            std::vector<uint32_t> indices;
        };
    
        std::vector<ChildShape> childShapes;

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
        std::unordered_set<Entity*> currentCollisions;
        std::unordered_set<Entity*> previousCollisions;
        
        // Use separate containers for delta tracking
        std::vector<Entity*> enters;
        std::vector<Entity*> exits;
        
        // For realism add drag and cross-section
        float dragCoefficient = 1.0f;
        float crossSectionArea = 1.0f;

        // Add callback for collision detection
        using CollisionCallback = std::function<void(Entity* other)>;
    
        struct CollisionEvents {
            CollisionCallback onEnter = nullptr;
            CollisionCallback onStay = nullptr;
            CollisionCallback onExit = nullptr;
        };

        CollisionEvents callbacks;

        virtual ~CollisionComponent();

        void freeBulletBody();
        void freeGhostObject();

        bool hasCallbacks() const { return callbacks.onEnter || callbacks.onStay || callbacks.onExit; }
        bool wantsEnter() const { return callbacks.onEnter != nullptr; }
        bool wantsStay() const { return callbacks.onStay != nullptr; }
        bool wantsExit() const { return callbacks.onExit != nullptr; }

        // The ID of this component type is "Collision"
        static std::string getID() { return "Collision"; }

        // Reads collision properties from the given json object
        void deserialize(const nlohmann::json& data) override;

        void loadModel(const std::string& path);
    };

}