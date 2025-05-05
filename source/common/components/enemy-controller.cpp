#include <systems/collision-system.hpp>
#include "enemy-controller.hpp"

namespace our {

    EnemyControllerComponent::~EnemyControllerComponent() {
        freeDetectionArea();

        if (characterController) {
            CollisionSystem::getInstance().getPhysicsWorld()->removeAction(characterController.get());
            characterController.reset();
        }
    }

    void EnemyControllerComponent::freeDetectionArea() {
        if (detectionArea) {
            CollisionSystem::getInstance().getPhysicsWorld()->removeCollisionObject(detectionArea.get());
            btCollisionShape* shape = detectionArea->getCollisionShape();
            if (shape) {
                delete shape;
            }
            detectionArea.reset();
        }
    }

    void EnemyControllerComponent::deserialize(const nlohmann::json& data) {
        movementSpeed = data.value("movementSpeed", 5.0f);
        stepHeight = data.value("stepHeight", 0.35f);
        detectionRadius = data.value("detectionRadius", 5.0f);
    }
}