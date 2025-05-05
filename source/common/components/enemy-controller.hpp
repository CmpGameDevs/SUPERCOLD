#pragma once
#include <ecs/component.hpp>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>


namespace our {
    enum class EnemyState {
        PATROLLING,
        CHASING,
        ATTACKING,
        SEARCHING
    };
    
    class EnemyControllerComponent : public Component {
    public:
        float movementSpeed;
        float stepHeight = 0.35f;
        float detectionRadius = 5.0f;
        std::unique_ptr<btKinematicCharacterController> characterController = nullptr;
        std::unique_ptr<btPairCachingGhostObject> detectionArea = nullptr;
        void* weapon = nullptr;
        float attackRange = 5.0f;
        float attackCooldown = 1.0f;
        EnemyState currentState = EnemyState::PATROLLING;
        glm::vec3 lastKnownPosition = glm::vec3(0.0f);
        float stateTimer = 0.0f;

        ~EnemyControllerComponent();

        static std::string getID() { return "Enemy Controller"; }

        void deserialize(const nlohmann::json& data) override;
    };
}