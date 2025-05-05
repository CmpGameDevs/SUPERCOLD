#pragma once
#include <ecs/transform.hpp>
#include <components/enemy-controller.hpp>
#include <components/fps-controller.hpp>
#include "collision-system.hpp"

namespace our {

class EnemySystem {
public:
    Entity* playerEntity = nullptr;
    CollisionSystem* collisionSystem = nullptr;

    void update(World* world, float deltaTime) {
        // Find player entity once
        if(!playerEntity) {
            for(auto entity : world->getEntities()) {
                if(entity->getComponent<FPSControllerComponent>()) {
                    playerEntity = entity;
                    break;
                }
            }
        }

        for(auto entity : world->getEntities()) {
            auto enemy = entity->getComponent<EnemyControllerComponent>();
            if(!enemy) continue;

            Transform transform = entity->localTransform;
            auto collision = entity->getComponent<CollisionComponent>();

            if(!collision) continue;

            updateAIState(enemy, &transform, collision, deltaTime);
            handleMovement(enemy, &transform, collision, deltaTime);
        }
    }

private:
    void updateAIState(EnemyControllerComponent* enemy, 
                      Transform* transform,
                      CollisionComponent* collision,
                      float deltaTime) {
        
        enemy->stateTimer += deltaTime;

        switch(enemy->currentState) {
            case EnemyState::PATROLLING:
                // handlePatrolling(enemy, transform, deltaTime);
                checkPlayerDetection(enemy, collision);
                break;
                
            case EnemyState::CHASING:
                // handleChasing(enemy, transform, deltaTime);
                checkAttackRange(enemy, transform);
                checkPlayerLoss(enemy, collision);
                break;
                
            case EnemyState::ATTACKING:
                handleAttacking(enemy, deltaTime);
                break;
                
            case EnemyState::SEARCHING:
                handleSearching(enemy, transform, deltaTime);
                break;
        }
    }

    void handleMovement(EnemyControllerComponent* enemy,
                       Transform* transform,
                       CollisionComponent* collision,
                       float deltaTime) {
        
        if(!playerEntity || !collision->ghostObject) return;

        glm::vec3 moveDirection(0.0f);
        
        switch(enemy->currentState) {
            case EnemyState::CHASING: {
                Transform playerTransform = playerEntity->localTransform;
                moveDirection = playerTransform.position - transform->position;
                moveDirection.y = 0; // Keep movement horizontal
                if(glm::length(moveDirection) > 0) {
                    moveDirection = glm::normalize(moveDirection);
                }
                break;
            }
            case EnemyState::PATROLLING:
                // Implement patrol path logic here
                break;
        }

        // Apply movement through collision system
        if(glm::length(moveDirection) > 0) {
            glm::vec3 velocity = moveDirection * enemy->movementSpeed;
            // collisionSystem->moveGhost(entity, velocity, deltaTime);
        }
    }

    void checkPlayerDetection(EnemyControllerComponent* enemy, 
                            CollisionComponent* collision) {
        
        if(!enemy->detectionArea) return;

        int numObjects = enemy->detectionArea->getNumOverlappingObjects();
        for(int i = 0; i < numObjects; ++i) {
            btCollisionObject* other = enemy->detectionArea->getOverlappingObject(i);
            if(Entity* e = static_cast<Entity*>(other->getUserPointer())) {
                if(e == playerEntity) {
                    enemy->currentState = EnemyState::CHASING;
                    enemy->lastKnownPosition = playerEntity->localTransform.position;
                }
            }
        }
    }

    void checkAttackRange(EnemyControllerComponent* enemy,
                        Transform* transform) {
        
        if(!playerEntity) return;
        
        auto playerTransform = playerEntity->localTransform;

        float distance = glm::distance(transform->position, playerTransform.position);
        if(distance <= enemy->attackRange) {
            enemy->currentState = EnemyState::ATTACKING;
            enemy->stateTimer = 0.0f;
        }
    }

    void handleAttacking(EnemyControllerComponent* enemy, float deltaTime) {
        // Implement attack logic here
        if(enemy->stateTimer >= enemy->attackCooldown) {
            // Perform attack
            enemy->stateTimer = 0.0f;
            
            // Return to chasing after attack
            enemy->currentState = EnemyState::CHASING;
        }
    }

    void checkPlayerLoss(EnemyControllerComponent* enemy,
                       CollisionComponent* collision) {
        
        if(!enemy->detectionArea) return;

        bool playerInSight = false;
        int numObjects = enemy->detectionArea->getNumOverlappingObjects();
        for(int i = 0; i < numObjects; ++i) {
            btCollisionObject* other = enemy->detectionArea->getOverlappingObject(i);
            if(Entity* e = static_cast<Entity*>(other->getUserPointer())) {
                if(e == playerEntity) {
                    playerInSight = true;
                    break;
                }
            }
        }

        if(!playerInSight) {
            enemy->currentState = EnemyState::SEARCHING;
            enemy->stateTimer = 0.0f;
        }
    }

    void handleSearching(EnemyControllerComponent* enemy,
                        Transform* transform,
                        float deltaTime) {
        
        // Move to last known position
        glm::vec3 toLastKnown = enemy->lastKnownPosition - transform->position;
        if(glm::length(toLastKnown) > 0.1f) {
            glm::vec3 moveDir = glm::normalize(toLastKnown);
            // collisionSystem->moveGhost(entity, moveDir * enemy->movementSpeed, deltaTime);
        } else {
            // Return to patrolling after search duration
            if(enemy->stateTimer > 5.0f) {
                enemy->currentState = EnemyState::PATROLLING;
            }
        }
    }
};

} // namespace our