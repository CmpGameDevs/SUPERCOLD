#pragma once
#include <btBulletDynamicsCommon.h>
#include "../ecs/entity.hpp"
#include <functional>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace our {

class CollisionSystem {
    btDiscreteDynamicsWorld* physicsWorld = nullptr;
    std::function<void(Entity*, Entity*)> onCollision;

public:
    // Initialize the collision system with a Bullet physics world
    void initialize(btDynamicsWorld* world);
    
    // Step the physics simulation by a given time step
    void stepSimulation(float deltaTime);

    // Update entity transforms and physics simulation
    void update(World* world, float deltaTime);
    
    // Collision callback
    void setCollisionCallback(std::function<void(Entity*, Entity*)> callback) { 
        onCollision = callback; 
    }
};

} // namespace our