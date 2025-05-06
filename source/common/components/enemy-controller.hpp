#pragma once
#include <ecs/component.hpp>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>


namespace our {
    enum class EnemyState {
        PATROLLING,
        CHASING,
        ATTACKING,
        SEARCHING,
        DEAD
    };
    
    class EnemyControllerComponent : public Component {
    public:
        bool initialized = false;
        float movementSpeed = 0.5f;
        float stepHeight = 0.35f;
        float detectionRadius = 10.0f;
        std::unique_ptr<btKinematicCharacterController> characterController = nullptr;
        std::unique_ptr<btPairCachingGhostObject> detectionArea = nullptr;
        Entity *weapon = nullptr;
        Entity *model = nullptr;
        float attackRange = 30.0f;
        float attackCooldown = 1.0f;
        float distanceToKeep = 15.0f;
        EnemyState currentState = EnemyState::PATROLLING;
        glm::vec3 moveDirection = glm::vec3(0.0f);
        glm::vec3 lastKnownPosition = glm::vec3(0.0f);
        float stateTimer = 0.0f;

        ~EnemyControllerComponent();

        void freeDetectionArea();

        static std::string getID() { return "Enemy Controller"; }

        void deserialize(const nlohmann::json& data) override;
    };
}