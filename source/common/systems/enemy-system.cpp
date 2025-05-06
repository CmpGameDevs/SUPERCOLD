#include "enemy-system.hpp"

namespace our {

void EnemySystem::update(World *world, float deltaTime) {
    // Find player entity once
    if (!playerEntity) {
        for (auto entity : world->getEntities()) {
            if (entity->getComponent<FPSControllerComponent>()) {
                playerEntity = entity;
                break;
            }
        }
    }

    for (auto entity : world->getEntities()) {
        auto enemy = entity->getComponent<EnemyControllerComponent>();
        if (!enemy)
            continue;

        Transform transform = entity->localTransform;
        auto collision = entity->getComponent<CollisionComponent>();

        if (!collision)
            continue;

        _updateAIState(entity, deltaTime);
        _handleMovement(entity, deltaTime);
    }
}

void EnemySystem::_updateAIState(Entity *entity, float deltaTime) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    enemy->stateTimer += deltaTime;

    switch (enemy->currentState) {
    case EnemyState::PATROLLING:
        _handlePatrolling(entity);
        _checkPlayerDetection(entity);
        break;

    case EnemyState::CHASING:
        _handleChasing(entity);
        _checkAttackRange(entity);
        _checkPlayerLoss(entity);
        break;

    case EnemyState::ATTACKING:
        _handleAttacking(entity, deltaTime);
        break;

    case EnemyState::SEARCHING:
        _handleSearching(entity, deltaTime);
        break;
    }
}

void EnemySystem::_handleMovement(Entity *entity, float deltaTime) {
    auto collision = entity->getComponent<CollisionComponent>();
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    if (!playerEntity || !collision->ghostObject) return;

    glm::vec3 moveDirection = enemy->moveDirection;

    // Apply movement through collision system
    float slowdownFactor = 0.1f;
    if (enemy->currentState == EnemyState::PATROLLING) {
        slowdownFactor = 0.05f;
    } else if (enemy->currentState == EnemyState::CHASING) {
        slowdownFactor = 0.2f;
    }

    glm::vec3 velocity = moveDirection * enemy->movementSpeed * slowdownFactor;
    collisionSystem->moveGhost(entity, velocity, deltaTime);
}

void EnemySystem::_handlePatrolling(Entity *entity) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    // If there is no detection area, update it.
    if (!enemy->detectionArea) {
        collisionSystem->createDetectionArea(entity);
    }
}

void EnemySystem::_checkPlayerDetection(Entity *entity) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    if (!enemy->detectionArea)
        return;

    int numObjects = enemy->detectionArea->getNumOverlappingObjects();
    for (int i = 0; i < numObjects; ++i) {
        btCollisionObject *other = enemy->detectionArea->getOverlappingObject(i);
        if (Entity *e = static_cast<Entity *>(other->getUserPointer())) {
            // Check if there is a direct line of sight between both entities
            if (e == playerEntity && _checkDirectSight(entity)) {
                // Make the detection area bigger
                enemy->freeDetectionArea();
                enemy->detectionRadius *= 5.0;
                collisionSystem->createDetectionArea(entity);

                enemy->currentState = EnemyState::CHASING;
                enemy->lastKnownPosition = playerEntity->localTransform.position;
                return;
            }
        }
    }
}

bool EnemySystem::_checkDirectSight(Entity *entity) {
    glm::vec3 playerPosition = playerEntity->localTransform.position;
    glm::vec3 enemyPosition = entity->localTransform.position;

    glm::vec3 rayStart = enemyPosition;
    glm::vec3 rayEnd = playerPosition;
    CollisionComponent *hitComponent = nullptr;
    glm::vec3 hitPoint, hitNormal;
    if (collisionSystem->raycast(rayStart, rayEnd, hitComponent, hitPoint, hitNormal)) {
        auto otherEntity = hitComponent->getOwner();
        if (otherEntity != playerEntity) return false;
    }
    return true;
}

void EnemySystem::_handleChasing(Entity *entity) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    Transform* transform = &entity->localTransform;
    Transform* playerTransform = &playerEntity->localTransform;
    float distance = glm::distance(transform->position, playerTransform->position);

    // Don't want the enemy to kiss the player yk
    if (distance < enemy->distanceToKeep) {
        enemy->moveDirection = glm::vec3(0.0f);
        return;
    }

    glm::vec3 moveDirection = playerTransform->position - transform->position;
    moveDirection.y = 0;
    if (glm::length(moveDirection) > 0) {
        moveDirection = glm::normalize(moveDirection);
    }
    enemy->moveDirection = moveDirection;
}

void EnemySystem::_checkAttackRange(Entity *entity) {
    if (!playerEntity) return;

    auto enemy = entity->getComponent<EnemyControllerComponent>();
    Transform* transform = &entity->localTransform;
    Transform* playerTransform = &playerEntity->localTransform;

    float distance = glm::distance(transform->position, playerTransform->position);
    if (distance <= enemy->attackRange) {
        enemy->currentState = EnemyState::ATTACKING;
        enemy->stateTimer = 0.0f;
    }
}

void EnemySystem::_handleAttacking(Entity *entity, float deltaTime) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();

    // TODO: get the weapon of the enemy and use WeaponsSystem.
    Entity* weapon = enemy->weapon;

    if (enemy->stateTimer >= enemy->attackCooldown) {
        enemy->currentState = EnemyState::CHASING;
        enemy->stateTimer = 0.0f;
    }
}

void EnemySystem::_checkPlayerLoss(Entity *entity) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    if (!enemy->detectionArea)
        return;

    bool playerInSight = false;
    int numObjects = enemy->detectionArea->getNumOverlappingObjects();
    for (int i = 0; i < numObjects; ++i) {
        btCollisionObject *other = enemy->detectionArea->getOverlappingObject(i);
        if (Entity *e = static_cast<Entity *>(other->getUserPointer())) {
            if (e == playerEntity && _checkDirectSight(entity)) {
                playerInSight = true;
                break;
            }
        }
    }

    if (!playerInSight) {
        enemy->currentState = EnemyState::SEARCHING;
        enemy->stateTimer = 0.0f;
    }
}

void EnemySystem::_handleSearching(Entity *entity, float deltaTime) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    Transform* transform = &entity->localTransform;

    // Move to last known position
    glm::vec3 toLastKnown = enemy->lastKnownPosition - transform->position;
    if (glm::length(toLastKnown) > 0.1f) {
        enemy->moveDirection = glm::normalize(toLastKnown);
    } else if (enemy->stateTimer > 10.0f) {
        // Return to patrolling after search duration
        // Make the detection area smaller again
        enemy->freeDetectionArea();
        enemy->detectionRadius /= 5.0;
        collisionSystem->createDetectionArea(entity);

        enemy->currentState = EnemyState::PATROLLING;
        enemy->stateTimer = 0.0f;
    }
}

}