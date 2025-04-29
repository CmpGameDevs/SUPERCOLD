#include "weapons-system.hpp"
#include <components/weapon.hpp>
#include <components/camera.hpp>
#include <components/collision.hpp>
#include <systems/collision-system.hpp>

namespace our {
    void WeaponsSystem::update(World* world, float deltaTime) {
    }
    
    void WeaponsSystem::throwWeapon(World* world, Entity* entity, glm::vec3 forward) {
        dropWeapon(world, entity);

        // Throw weapon to the front of the player
        WeaponComponent* weapon = entity->getComponent<WeaponComponent>();
        glm::vec3 throwDirection = forward * 10.0f * weapon->throwForce;
        CollisionSystem::getInstance().applyVelocity(entity, throwDirection);
        weaponsMap.erase(entity);
    }
    
    void WeaponsSystem::dropWeapon(World* world, Entity* entity) {
        if (!entity) return;
        WeaponComponent* weapon = entity->getComponent<WeaponComponent>();
        if (!weapon) return;
        // Set the weapon's position and rotation to match the world's
        glm::mat4 worldMatrix = entity->getLocalToWorldMatrix();
        entity->parent = nullptr;
        entity->localTransform.position = glm::vec3(worldMatrix[3]);
        entity->localTransform.rotation = glm::vec3(worldMatrix[2]);
        entity->localTransform.scale = glm::vec3(glm::length(worldMatrix[0]), glm::length(worldMatrix[1]), glm::length(worldMatrix[2]));

        CollisionComponent* collision = static_cast<CollisionComponent*>(_addCollisionComponent(entity));
        weaponsMap.erase(entity);
    }
    
    void WeaponsSystem::reloadWeapon(World* world, Entity* entity) {
    }
    
    void WeaponsSystem::fireWeapon(World* world, Entity* entity, glm::vec3 direction) {
    }
    
    void WeaponsSystem::pickupWeapon(World* world, Entity* entity, Entity* weaponEntity) {
        if (!entity || !weaponEntity) return;
        WeaponComponent* weapon = weaponEntity->getComponent<WeaponComponent>();
        if (!weapon) return;
        // Remove rigid body from the weapon entity
        _removeCollisionComponent(weaponEntity);
        // Attach the weapon to the player entity
        weaponEntity->parent = entity;
        // Set the weapon's position and rotation to match the player's
        weaponEntity->localTransform.position = glm::vec3(0.4f, -0.2f, -0.4f);
        weaponEntity->localTransform.rotation = glm::vec3(0, 0, 0);
    }
    
    void WeaponsSystem::_removeCollisionComponent(Entity* entity) {
        if (!entity) return;
        CollisionComponent* collision = entity->getComponent<CollisionComponent>();
        if (collision) collision->freeBulletBody();
        entity->removeComponent(collision);
        if (weaponsMap.count(entity)) {
            if (weaponsMap[entity] != collision)
                delete weaponsMap[entity];
            weaponsMap.erase(entity);
        }
        weaponsMap[entity] = collision;
    }

    Component *WeaponsSystem::_addCollisionComponent(Entity* entity) {
        CollisionComponent* collision = static_cast<CollisionComponent*>(weaponsMap[entity]);
        // Calculate the world matrix from the entity's local transform
        glm::mat4 worldMatrix = entity->getLocalToWorldMatrix();
        Transform transform;
        transform.position = glm::vec3(worldMatrix[3]);
        transform.rotation = entity->localTransform.rotation;
        transform.scale = glm::vec3(glm::length(worldMatrix[0]), glm::length(worldMatrix[1]), glm::length(worldMatrix[2]));

        CollisionSystem::getInstance().createRigidBody(entity, collision, &transform);

        entity->addComponent(collision);
        return collision;
    }
}