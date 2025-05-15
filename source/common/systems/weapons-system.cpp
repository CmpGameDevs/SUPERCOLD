#include "weapons-system.hpp"
#include <components/camera.hpp>
#include <components/collision.hpp>
#include <components/enemy-controller.hpp>
#include <components/model-renderer.hpp>
#include <components/weapon.hpp>
#include <systems/audio-system.hpp>
#include <systems/collision-system.hpp>
#include <systems/fps-controller.hpp>
#include <systems/movement.hpp>

namespace our {
void WeaponsSystem::update(World *world, float deltaTime) {
    std::vector<Projectile *> toRemove;
    toRemove.reserve(projectiles.size());

    for (auto *proj : projectiles) {
        if (!proj->entity) {
            toRemove.push_back(proj);
            continue;
        }
        proj->timeAlive += deltaTime;

        if (proj->lifetime > 0.0f && proj->timeAlive >= proj->lifetime) {
            world->markForRemoval(proj->entity);
            toRemove.push_back(proj);
        }
    }
    world->deleteMarkedEntities();

    for (auto *deadProj : toRemove) {
        projectiles.erase(deadProj);
        delete deadProj;
    }
}

bool WeaponsSystem::throwWeapon(Entity *entity, glm::vec3 forward) {
    if (!dropWeapon(entity))
        return false;
    WeaponComponent *weapon = entity->getComponent<WeaponComponent>();
    glm::vec3 throwDirection = forward * 10.0f * weapon->throwForce;
    CollisionSystem::getInstance().applyVelocity(entity, throwDirection);
    weaponsMap.erase(entity);
    glm::vec3 globalPosition = entity->getLocalToWorldMatrix()[3];
    AudioSystem::getInstance().playSpatialSound("throwing", entity, globalPosition, "sfx", false, 1.0f, 100.0f);
    return true;
}

bool WeaponsSystem::dropWeapon(Entity *entity) {
    if (!entity)
        return false;
    WeaponComponent *weapon = entity->getComponent<WeaponComponent>();
    if (!weapon)
        return false;
    glm::mat4 worldMatrix = entity->getLocalToWorldMatrix();
    entity->parent = nullptr;
    entity->localTransform.position = glm::vec3(worldMatrix[3]);
    entity->localTransform.rotation = glm::vec3(worldMatrix[2]);
    entity->localTransform.scale =
        glm::vec3(glm::length(worldMatrix[0]), glm::length(worldMatrix[1]), glm::length(worldMatrix[2]));
    CollisionComponent *collision = static_cast<CollisionComponent *>(_addCollisionComponent(entity));
    weaponsMap.erase(entity);
    return true;
}

bool WeaponsSystem::reloadWeapon(Entity *entity) {
    if (!entity)
        return false;
    WeaponComponent *weapon = entity->getComponent<WeaponComponent>();
    if (!weapon || weapon->fireCooldown > 0.0f)
        return false;
    weapon->currentAmmo = weapon->ammoCapacity;
    weapon->fireCooldown = weapon->reloadTime;
    glm::vec3 globalPosition = entity->getLocalToWorldMatrix()[3];
    AudioSystem::getInstance().playSpatialSound("gun_reloading", entity, globalPosition, "sfx", false, 1.0f, 100.0f);
    return true;
}

bool WeaponsSystem::fireWeapon(World *world, Entity *entity, glm::vec3 direction, glm::mat4 viewMatrix,
                               glm::mat4 projectionMatrix) {
    if (!entity)
        return false;
    WeaponComponent *weapon = entity->getComponent<WeaponComponent>();
    if (!weapon)
        return false;
    if (weapon->fireCooldown > 0.0f)
        return false;
    if (weapon->currentAmmo <= 0) {
        glm::vec3 globalPosition = entity->getLocalToWorldMatrix()[3];
        AudioSystem::getInstance().playSpatialSound("gun_empty", entity, globalPosition, "sfx", false, 1.0f, 100.0f);
        return false;
    }

    float speed = 10.0f;
    float lifetime = 5.0f;
    Entity *projectileEntity = _createProjectile(world, entity, direction, speed, viewMatrix, projectionMatrix);
    Projectile *projectile = new Projectile(projectileEntity, entity, direction, speed, weapon->range, lifetime);
    projectiles.insert(projectile);
    weapon->currentAmmo--;
    glm::vec3 globalPosition = entity->getLocalToWorldMatrix()[3];
    AudioSystem::getInstance().playSpatialSound("gunshot", entity, globalPosition, "sfx", false, 1.0f, 100.0f);
    weapon->fireCooldown = weapon->fireRate;
    return true;
}

bool WeaponsSystem::pickupWeapon(Entity *entity, Entity *weaponEntity) {
    if (!entity || !weaponEntity)
        return false;
    WeaponComponent *weapon = weaponEntity->getComponent<WeaponComponent>();
    if (!weapon)
        return false;
    _removeCollisionComponent(weaponEntity);
    weaponEntity->parent = entity;
    weaponEntity->localTransform.position = weapon->weaponPosition;
    weaponEntity->localTransform.rotation = weapon->weaponRotation;
    return true;
}

void WeaponsSystem::_removeCollisionComponent(Entity *entity) {
    if (!entity)
        return;
    CollisionComponent *collision = entity->getComponent<CollisionComponent>();
    if (collision)
        collision->freeBulletBody();
    entity->removeComponent(collision);
    if (weaponsMap.count(entity)) {
        if (weaponsMap[entity] != collision)
            delete weaponsMap[entity];
        weaponsMap.erase(entity);
    }
    weaponsMap[entity] = collision;
}

Component *WeaponsSystem::_addCollisionComponent(Entity *entity) {
    CollisionComponent *collision = nullptr;
    if (weaponsMap.count(entity)) {
        collision = static_cast<CollisionComponent *>(weaponsMap[entity]);
    } else {
        collision = new CollisionComponent();
        collision->shape = CollisionShape::MESH;
        collision->loadModel(entity->name);
        collision->mass = 1.0f;
    }

    glm::mat4 worldMatrix = entity->getLocalToWorldMatrix();
    Transform transform;
    transform.position = glm::vec3(worldMatrix[3]);
    transform.rotation = entity->localTransform.rotation;
    transform.scale = glm::vec3(glm::length(worldMatrix[0]), glm::length(worldMatrix[1]), glm::length(worldMatrix[2]));
    CollisionSystem::getInstance().createRigidBody(entity, collision, &transform);
    entity->addComponent(collision);
    return collision;
}

glm::vec3 getCrosshairDirection(glm::mat4 viewMatrix, glm::mat4 projectionMatrix) {
    glm::vec2 screenCenter = glm::vec2(0.5f, 0.5f);
    glm::vec4 rayClip(screenCenter.x * 2.0f - 1.0f, screenCenter.y * 2.0f - 1.0f, 1.0f, 1.0f);
    glm::vec4 rayEye = glm::inverse(projectionMatrix) * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
    return glm::normalize(glm::vec3(glm::inverse(viewMatrix) * rayEye));
}

Entity *WeaponsSystem::_createProjectile(World *world, Entity *owner, glm::vec3 direction, float speed,
                                         glm::mat4 viewMatrix, glm::mat4 projectionMatrix) {
    Entity *projectileEntity = world->add();
    bool isPlayer = owner->parent->getComponent<FPSControllerComponent>() != nullptr;
    projectileEntity->name = isPlayer ? "Projectile" : "EnemyProjectile";

    WeaponComponent *weapon = owner->getComponent<WeaponComponent>();
    glm::mat4 worldMatrix = owner->getLocalToWorldMatrix();

    glm::vec3 weaponForward = glm::normalize(glm::vec3(worldMatrix[2]));
    glm::vec3 weaponRight = glm::normalize(glm::vec3(worldMatrix[0]));
    glm::vec3 muzzlePosition = glm::vec3(worldMatrix[3]) + weaponForward * weapon->muzzleForwardOffset +
                               weaponRight * weapon->muzzleRightOffset;

    glm::quat bulletRotation = owner->parent->localTransform.rotation * weapon->bulletRotation;

    projectileEntity->localTransform.position = muzzlePosition;
    projectileEntity->localTransform.scale = weapon->bulletScale;
    projectileEntity->localTransform.rotation = bulletRotation;

    glm::vec3 cameraPosition = glm::vec3(glm::inverse(viewMatrix)[3]);
    glm::vec3 crosshairDir = getCrosshairDirection(viewMatrix, projectionMatrix);

    auto *movement = projectileEntity->addComponent<MovementComponent>();

    // Cast a ray from the camera to the target point
    glm::vec3 rayStart = cameraPosition;
    glm::vec3 rayEnd = rayStart + direction * weapon->range;
    CollisionComponent *hitComponent = nullptr;
    glm::vec3 hitPoint, hitNormal;
    glm::vec3 bulletDirection;
    CollisionSystem *collisionSystem = &CollisionSystem::getInstance();
    if (collisionSystem->raycast(rayStart, rayEnd, hitComponent, hitPoint, hitNormal)) {
        // Update the direction of the bullet
        bulletDirection = glm::normalize(hitPoint - muzzlePosition);
    } else {
        // If no hit, use the crosshair direction
        bulletDirection = glm::normalize((cameraPosition + crosshairDir * weapon->range) - muzzlePosition);
    }

    movement->linearVelocity = bulletDirection * speed;

    CollisionComponent *collision = projectileEntity->addComponent<CollisionComponent>();
    if (weapon->model) {
        ModelComponent *modelRenderer = projectileEntity->addComponent<ModelComponent>();
        modelRenderer->model = weapon->model;
        collision->shape = CollisionShape::MESH;
        Mesh *mesh = weapon->model->getCombinedMesh();
        if (mesh) {
            CollisionComponent::ChildShape childShape;
            childShape.shape = CollisionShape::MESH;
            childShape.vertices = mesh->cpuVertices; // already in world space
            childShape.indices = mesh->cpuIndices;
            collision->childShapes.push_back(childShape);
        }
    } else {
        collision->shape = CollisionShape::SPHERE;
    }
    collision->halfExtents = glm::vec3(1.0f * weapon->bulletSize);
    collision->mass = 0;
    collision->isKinematic = true;
    collision->callbacks.onEnter = [this, projectileEntity, world](Entity *other) {
        if (other->name == "Projectile")
            return;
        std::string name = projectileEntity->name;
        std::string otherName = other->name;
        if (name == otherName)
            return;
        if (other->getComponent<EnemyControllerComponent>() != nullptr && name == "EnemyProjectile")
            return;
        if (other->getComponent<FPSControllerComponent>() != nullptr && name == "Projectile")
            return;
        world->markForRemoval(projectileEntity);
    };
    return projectileEntity;
}

void WeaponsSystem::onDestroy() {
    for (auto *proj : projectiles) {
        delete proj;
    }
    projectiles.clear();
}
} // namespace our
