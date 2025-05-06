#include <components/weapon.hpp>
#include <components/enemy-controller.hpp>
#include <components/fps-controller.hpp>
#include <systems/weapons-system.hpp>
#include <systems/audio-system.hpp>
#include <components/model-renderer.hpp>
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
        if (auto weapon = entity->getComponent<WeaponComponent>()) {
            _setEnemyWeapon(entity->parent, entity);
            continue;
        }

        if(auto model = entity->getComponent<ModelComponent>()) {
           _setEnemyModel(entity->parent, entity);
           continue;
        }

        auto enemy = entity->getComponent<EnemyControllerComponent>();
        if (!enemy) continue;
        _setCollisionCallbacks(entity);

        Transform transform = entity->localTransform;
        auto collision = entity->getComponent<CollisionComponent>();

        if (!collision) continue;

        _updateAIState(entity, deltaTime);
        _handleMovement(entity, deltaTime);
    }
}

void EnemySystem::_setEnemyWeapon(Entity *entity, Entity *weaponEntity) {
    WeaponComponent* weapon = weaponEntity->getComponent<WeaponComponent>();
    if (!entity || !weapon) return;
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    if (!enemy || enemy->weapon) return;
    enemy->weapon = weaponEntity;
    weaponEntity->localTransform.position = glm::vec3(0.6f, -0.2f, -0.4f);
    weaponEntity->localTransform.rotation = weapon->weaponRotation;
}

void EnemySystem::_setEnemyModel(Entity *entity, Entity *modelEntity) {
    ModelComponent* model = modelEntity->getComponent<ModelComponent>();
    if (!entity || !model) return;
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    if (!enemy || enemy->model) return;
    enemy->model = modelEntity;
}

void EnemySystem::_setCollisionCallbacks(Entity *entity) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    if (enemy->initialized) return;
    auto collision = entity->getComponent<CollisionComponent>();
    if (!collision) return;

    collision->callbacks.onEnter = [enemy, entity](Entity *other) {
        if (other->name == "Projectile") { 
            AudioSystem::getInstance().playSpatialSound("killing", entity, entity->localTransform.position, "sfx", false, 1.0f, 100.0f);
            enemy->currentState = EnemyState::DEAD;
            enemy->stateTimer = 0.0f;
        }
    };

    enemy->initialized = true;
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
    
    case EnemyState::DEAD:
        _handleDeath(entity);
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
        printf("Creating detection area of radius %f\n", enemy->detectionRadius);
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
    // Update rotation to look at the player
    glm::vec3 direction = playerTransform->position - transform->position;
    if (glm::length(direction) > 0) {
        direction.y = 0;
        direction = glm::normalize(direction);
        transform->rotation = glm::quatLookAt(direction, glm::vec3(0, 1, 0));
    }

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
    if (distance <= enemy->attackRange && enemy->stateTimer >= enemy->attackCooldown) {
        enemy->currentState = EnemyState::ATTACKING;
        enemy->stateTimer = 0.0f;
    }
}

void EnemySystem::_handleAttacking(Entity *entity, float deltaTime) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();

    if (Entity* weaponEntity = enemy->weapon) {
        auto worldMatrix = entity->getLocalToWorldMatrix();
        glm::vec3 weaponForward = -glm::normalize(glm::vec3(worldMatrix[2]));
        auto viewMatrix = glm::inverse(worldMatrix);
        auto projectionMatrix = glm::perspective(glm::radians(45.0f), 1.0f, 0.1f, 100.0f);
        auto weapon = weaponEntity->getComponent<WeaponComponent>();
        weapon->fireCooldown -= deltaTime;
        if (weapon->currentAmmo <= 0) {
            WeaponsSystem::getInstance().reloadWeapon(weaponEntity);
        } else {
            WeaponsSystem::getInstance().fireWeapon(entity->getWorld(), weaponEntity, weaponForward, viewMatrix, projectionMatrix);
        }
    }

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
    if (glm::length(toLastKnown) > 0.5f) {
        enemy->moveDirection = glm::normalize(toLastKnown);
    } else if (enemy->stateTimer > 10.0f) {
        // Return to patrolling after search duration
        // Make the detection area smaller again
        enemy->freeDetectionArea();
        enemy->detectionRadius /= 5.0;
        collisionSystem->createDetectionArea(entity);

        enemy->currentState = EnemyState::PATROLLING;
        enemy->stateTimer = 0.0f;
    } else {
        // Stop moving if close to last known position
        enemy->moveDirection = glm::vec3(0.0f);
    }
}

void EnemySystem::_handleDeath(Entity *entity) {
    auto enemy = entity->getComponent<EnemyControllerComponent>();
    auto weaponEntity = enemy->weapon;
    if (weaponEntity) {
        // Throw the weapon to the direction of the enemy's forward vector
        auto worldMatrix = entity->getLocalToWorldMatrix();
        glm::vec3 weaponForward = -glm::normalize(glm::vec3(worldMatrix[2]));
        if (WeaponsSystem::getInstance().throwWeapon(weaponEntity, weaponForward)) {
            enemy->weapon = nullptr;
        }
    }
    auto modelEntity = enemy->model;
    if(modelEntity){
        modelEntity->getWorld()->markForRemoval(modelEntity);
    }
    enemy->moveDirection = glm::vec3(0.0f);
    if (enemy->stateTimer >= 1.5f) {
        entity->getWorld()->markForRemoval(entity);
    }
}

}