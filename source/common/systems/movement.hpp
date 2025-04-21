#pragma once

#include "../ecs/world.hpp"
#include "../components/movement.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/fast_trigonometry.hpp>

namespace our
{

    // The movement system is responsible for moving every entity which contains a MovementComponent.
    // This system is added as a simple example for how use the ECS framework to implement logic. 
    // For more information, see "common/components/movement.hpp"
    class MovementSystem {
    public:

        // This should be called every frame to update all entities containing a MovementComponent. 
        void update(World* world, float deltaTime) {
            for (auto entity : world->getEntities()) {
                MovementComponent* movement = entity->getComponent<MovementComponent>();
                if (!movement) continue;
        
                // Apply linear velocity to position
                entity->localTransform.position += movement->linearVelocity * deltaTime;
        
                // Apply angular velocity
                glm::vec3 angular = movement->angularVelocity;
                float angle = glm::length(angular);
        
                if (angle > 1e-5f) { // Avoid tiny rotations / division by zero
                    glm::vec3 axis = glm::normalize(angular);
                    float rotationAngle = angle * deltaTime;
        
                    glm::quat deltaRotation = glm::angleAxis(rotationAngle, axis);
                    entity->localTransform.rotation = glm::normalize(deltaRotation * entity->localTransform.rotation);
                }
            }
        }

    };

}
