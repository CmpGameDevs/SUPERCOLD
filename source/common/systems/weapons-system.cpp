#include "weapons-system.hpp"
#include <components/weapon.hpp>
#include <components/camera.hpp>
#include <components/collision.hpp>
#include <systems/collision-system.hpp>

namespace our {
    void WeaponsSystem::update(World* world, float deltaTime) {
        std::vector<Projectile*> toRemove;
        toRemove.reserve(projectiles.size());

        for(auto* proj : projectiles) {
            if(!proj->entity) {
                toRemove.push_back(proj);
                continue;
            }
            proj->entity->localTransform.position += proj->direction * proj->speed * deltaTime;
            proj->timeAlive += deltaTime;

            if(proj->lifetime > 0.0f && proj->timeAlive >= proj->lifetime) {
                printf("Projectile lifetime expired.\n");
                world->markForRemoval(proj->entity);
                toRemove.push_back(proj);
            }
        }
        world->deleteMarkedEntities();

        for(auto* deadProj : toRemove) {
            projectiles.erase(deadProj);
            delete deadProj;
        }
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
        if (!entity) return;
        WeaponComponent* weapon = entity->getComponent<WeaponComponent>();
        if (!weapon) return;
        // Reload the weapon's ammo
        weapon->currentAmmo = weapon->ammoCapacity;
        // TODO: Add a reload animation or sound
        // Add a cooldown to prevent spamming the reload action
        weapon->fireCooldown = 0.3f;
        printf("Reloaded weapon! Current ammo: %d\n", weapon->currentAmmo);
    }
    
    void WeaponsSystem::fireWeapon(World* world, Entity* entity, glm::vec3 direction) {
        if (!entity) return;
        WeaponComponent* weapon = entity->getComponent<WeaponComponent>();
        if (!weapon) return;
        // Check if the weapon has ammo and if the fire cooldown is active
        if (weapon->currentAmmo <= 0 || weapon->fireCooldown > 0.0f) return;
        float speed = 5.0f;
        float lifetime = 5.0f;
        // Create a projectile entity
        Entity* projectileEntity = _createProjectile(world, entity, direction, speed);
        Projectile *projectile = new Projectile(projectileEntity, entity, direction, speed, weapon->range, lifetime);
        projectiles.insert(projectile);
        weapon->currentAmmo--;
        printf("Fired weapon! Remaining ammo: %d\n", weapon->currentAmmo);
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

    Entity *WeaponsSystem::_createProjectile(World* world, Entity* owner, glm::vec3 direction, float speed) {
        Entity* projectileEntity = world->add();
        projectileEntity->name = "Projectile";
        // Set Transform
        glm::mat4 worldMatrix = owner->getLocalToWorldMatrix();
        projectileEntity->localTransform.position = glm::vec3(worldMatrix[3]);
        projectileEntity->localTransform.scale = glm::vec3(glm::length(worldMatrix[0]), glm::length(worldMatrix[1]), glm::length(worldMatrix[2]));
        // Set Projectile Properties
        CollisionComponent* collision = projectileEntity->addComponent<CollisionComponent>();
        collision->shape = CollisionShape::SPHERE;
        collision->halfExtents = glm::vec3(0.2f);
        collision->mass = 0;
        collision->isKinematic = true;
        collision->callbacks.onEnter = [this, projectileEntity, world](Entity* other) {
            if (other->name == "Projectile") return;
            world->markForRemoval(projectileEntity);
        };
        return projectileEntity;
    }

    void WeaponsSystem::onDestroy() {
        for (auto* proj : projectiles) {
            delete proj;
        }
        projectiles.clear();
    }
}