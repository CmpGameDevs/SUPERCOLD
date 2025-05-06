#pragma once
#include "collision-system.hpp"
#include <ecs/transform.hpp>

namespace our {

class EnemySystem {
    Entity *playerEntity = nullptr;
    CollisionSystem *collisionSystem = nullptr;

    // Private constructor to prevent instantiation
    EnemySystem() = default;
    EnemySystem(const EnemySystem&) = delete; // Prevent copying
    EnemySystem& operator=(const EnemySystem&) = delete; // Prevent assignment
    EnemySystem(EnemySystem&&) = delete; // Prevent moving
    EnemySystem& operator=(EnemySystem&&) = delete; // Prevent moving assignment

    void _setEnemyWeapon(Entity *entity, Entity *weaponEntity);

    void _setCollisionCallbacks(Entity *entity);

    void _updateAIState(Entity *entity, float deltaTime);

    void _handleMovement(Entity *entity, float deltaTime);

    void _handlePatrolling(Entity *entity);

    void _checkPlayerDetection(Entity *entity);

    bool _checkDirectSight(Entity *entity);

    void _handleChasing(Entity *entity);

    void _checkAttackRange(Entity *entity);

    void _handleAttacking(Entity *entity, float deltaTime);

    void _checkPlayerLoss(Entity *entity);

    void _handleSearching(Entity *entity, float deltaTime);

    void _handleDeath(Entity *entity);
public:
    static EnemySystem& getInstance() {
        static EnemySystem instance;
        return instance;
    }

    void setCollisionSystem() {
        collisionSystem = &CollisionSystem::getInstance();
    }

    void update(World *world, float deltaTime);

};

}