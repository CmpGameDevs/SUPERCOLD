#include "collision-system.hpp"
#include "../ecs/transform.hpp"
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btShapeHull.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace our {

    void CollisionSystem::initialize(glm::ivec2 windowSize, btDynamicsWorld* physicsWorld) {
        this->physicsWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld);
        this->debugDrawer = new GLDebugDrawer(windowSize);
        this->physicsWorld->setDebugDrawer(debugDrawer);
        if (this->physicsWorld->getDebugDrawer())
            this->physicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_DrawWireframe);
        this->physicsWorld->setGravity(btVector3(0, 0, 0));
        this->physicsWorld->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(
            new btGhostPairCallback()
        );
        this->physicsWorld->setInternalTickCallback(
            CollisionSystem::_simulateDrag,
            nullptr,
            true // isPreTick
        );
    }

    void CollisionSystem::_simulateDrag(btDynamicsWorld* world, btScalar deltaTime) {
        const float airDensity = 1.2f;

        for (int i = 0; i < world->getNumCollisionObjects(); ++i) {
            btCollisionObject* obj = world->getCollisionObjectArray()[i];
            btRigidBody* rb = btRigidBody::upcast(obj);
            if (!rb || rb->getInvMass() == 0.0f) continue;

            float mass = 1.0f / rb->getInvMass();

            // Look up our custom drag settings
            Entity* entity = static_cast<Entity*>(rb->getUserPointer());
            if (!entity) continue;

            auto* cc = entity->getComponent<CollisionComponent>();
            if (!cc) continue;

            float Cd = cc->dragCoefficient;
            float A  = cc->crossSectionArea;

            // Apply gravity: F = m * g
            rb->applyCentralForce(btVector3(0, -9.81f * mass, 0));

            // Apply quadratic drag: F = -½ * ρ * C_d * A * |v| * v
            btVector3 v = rb->getLinearVelocity();
            float speed = v.length();
            if (speed > SIMD_EPSILON) {
                float Fd = 0.5f * airDensity * Cd * A * speed * speed;
                btVector3 drag = v.normalized() * (-Fd);
                rb->applyCentralForce(drag);
            }
        }
    }

    void CollisionSystem::_stepSimulation(float deltaTime) {
        if (physicsWorld) {
            physicsWorld->stepSimulation(deltaTime);
        } else {
            printf("[ERROR] CollisionSystem::stepSimulation: physicsWorld is null!\n");
        }
    }

    void CollisionSystem::_processEntities(World* world) {
        for(auto entity : world->getEntities()) {
            auto collision = entity->getComponent<CollisionComponent>();
            if(!collision) continue;

            glm::mat4 worldMatrix = entity->getLocalToWorldMatrix();
            // Get transforfom from the world matrix
            Transform transform;
            transform.position = glm::vec3(worldMatrix[3]);
            transform.rotation = entity->localTransform.rotation;
            transform.scale = glm::vec3(glm::length(worldMatrix[0]), glm::length(worldMatrix[1]), glm::length(worldMatrix[2]));
            
            if(!collision->bulletBody && !collision->ghostObject) {
                _createRigidBody(entity, collision, &transform);
            }
            _syncTransforms(entity, collision, &transform);
            if (!collision->isKinematic) {
                Transform *localTransform = &entity->localTransform;
                localTransform->position = transform.position;
                localTransform->rotation = transform.rotation;
                localTransform->scale = transform.scale;
            }
        }
    }

    btCollisionShape* CollisionSystem::_createMeshShape(CollisionComponent* collision, const Transform* transform) {
        if (!collision->triangleMesh && collision->mass <= 0.0f && !collision->isKinematic) {
            // Build triangle mesh from vertices/indices
            collision->triangleMesh = new btTriangleMesh();
            for (size_t i = 0; i < collision->indices.size(); i += 3) {
                auto& v0 = collision->vertices[collision->indices[i]].position;
                auto& v1 = collision->vertices[collision->indices[i+1]].position;
                auto& v2 = collision->vertices[collision->indices[i+2]].position;
                
                collision->triangleMesh->addTriangle(
                    btVector3(v0.x, v0.y, v0.z),
                    btVector3(v1.x, v1.y, v1.z),
                    btVector3(v2.x, v2.y, v2.z)
                );
            }
            btCollisionShape* shape = new btBvhTriangleMeshShape(collision->triangleMesh, true, true);
            shape->setLocalScaling(btVector3(
                transform->scale.x,
                transform->scale.y,
                transform->scale.z
            ));
            return shape;
        } else {
            // For dynamic objects, create convex hull
            auto* convexRaw = new btConvexHullShape(
                (btScalar*)&collision->vertices[0].position,
                (int)collision->vertices.size(),
                sizeof(our::Vertex)
            );
            convexRaw->setMargin(0.01f);
            
            // Simplify using btShapeHull
            btShapeHull* hull = new btShapeHull(convexRaw);
            hull->buildHull(convexRaw->getMargin());
            
            btConvexHullShape* simplified = new btConvexHullShape();
            for (int i = 0; i < hull->numVertices(); ++i) {
                simplified->addPoint(hull->getVertexPointer()[i]);
            }
            
            delete convexRaw;
            delete hull;
            return simplified;
        }
    }

    void CollisionSystem::_createGhostObject(Entity* entity, CollisionComponent* collision, const Transform* transform) {
        if (collision->ghostObject) return;
        btCollisionShape* shape = new btCapsuleShape(
            collision->halfExtents.x,
            collision->halfExtents.y * 2.0f
        );
        btPairCachingGhostObject* ghost = new btPairCachingGhostObject();
        ghost->setCollisionShape(shape);
        ghost->setUserPointer(entity);
        btTransform btTrans;
        btTrans.setIdentity();
        btTrans.setOrigin(btVector3(
            transform->position.x,
            transform->position.y,
            transform->position.z
        ));
        ghost->setWorldTransform(btTrans);
        ghost->setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
        physicsWorld->addCollisionObject(
            ghost,
            btBroadphaseProxy::KinematicFilter,  // Group: treat player as kinematic
            btBroadphaseProxy::StaticFilter |    // Mask: collide with static
            btBroadphaseProxy::KinematicFilter | // and kinematic objects
            btBroadphaseProxy::DefaultFilter     // and default objects
        );
        physicsWorld->getBroadphase()->getOverlappingPairCache()->setInternalGhostPairCallback(
            new btGhostPairCallback()
        );
        physicsWorld->updateSingleAabb(ghost);
        collision->ghostObject = ghost;
    }

    void CollisionSystem::_createRigidBody(Entity* entity, CollisionComponent* collision, const Transform* transform) {
        btCollisionShape* shape = nullptr;
        if (collision->isKinematic) collision->mass = 0.0f;
                    
        // Create shape based on component data
        switch(collision->shape) {
            case CollisionShape::BOX:
                shape = new btBoxShape(btVector3(
                    collision->halfExtents.x,
                    collision->halfExtents.y,
                    collision->halfExtents.z
                ));
                break;
            case CollisionShape::SPHERE:
                shape = new btSphereShape(collision->halfExtents.x);
                break;
            case CollisionShape::CAPSULE:
                shape = new btCapsuleShape(
                    collision->halfExtents.x,  // radius
                    collision->halfExtents.y    // height
                );
                break;
            case CollisionShape::MESH:
                shape = _createMeshShape(collision, transform);
                break;
            case CollisionShape::GHOST:
                _createGhostObject(entity, collision, transform);
                return; // No need to create a rigid body for ghost objects
        }

        btTransform btTrans;
        btTrans.setIdentity();
        btTrans.setOrigin(btVector3(
            transform->position.x,
            transform->position.y,
            transform->position.z
        ));
        btVector3 inertia(0, 0, 0);
        if(collision->mass > 0) shape->calculateLocalInertia(collision->mass, inertia);
        
        btMotionState* motionState = new btDefaultMotionState(btTrans);
        btRigidBody::btRigidBodyConstructionInfo rbInfo(
            collision->mass, 
            motionState,
            shape,
            inertia
        );
        collision->bulletBody = new btRigidBody(rbInfo);
        collision->bulletBody->setUserPointer(entity);

        if (collision->isKinematic) {
            collision->bulletBody->setCollisionFlags(collision->bulletBody->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            collision->bulletBody->setActivationState(DISABLE_DEACTIVATION);
        }

        // Add a damping to the body if it has mass to simulate air resistance
        if (collision->mass > 0.0f) {
            btScalar linearDamping = 0.1f / collision->mass; 
            btScalar angularDamping = 0.05f / collision->mass;
            collision->bulletBody->setDamping(linearDamping, angularDamping);
        }

        int collisionGroup = collision->isKinematic ? btBroadphaseProxy::KinematicFilter : 
                     (collision->mass > 0 ? btBroadphaseProxy::DefaultFilter : btBroadphaseProxy::StaticFilter);
        int collisionMask = collision->isKinematic ? (btBroadphaseProxy::StaticFilter | btBroadphaseProxy::DefaultFilter) :
                     (btBroadphaseProxy::AllFilter);
        physicsWorld->addRigidBody(collision->bulletBody, collisionGroup, collisionMask);
    }

    void CollisionSystem::_syncTransforms(Entity* entity, CollisionComponent* collision, Transform* transform) {
        // Skip static objects
        if (collision->mass == 0.0f && !collision->isKinematic) return;

        if (collision->shape == CollisionShape::GHOST) {
            if (!collision->ghostObject) return;
            btTransform t = collision->ghostObject->getWorldTransform();
            transform->position = glm::vec3(t.getOrigin().x(), t.getOrigin().y(), t.getOrigin().z());
            return;
        }

        if(collision->isKinematic) {
            // Update physics from ECS
            btTransform trans;
            trans.setIdentity();
            trans.setOrigin(btVector3(
                transform->position.x,
                transform->position.y,
                transform->position.z
            ));
            // Convert Euler angles (pitch, yaw, roll) to quaternion
            btQuaternion quatPitch(btVector3(1, 0, 0), transform->rotation.x); // X-axis (pitch)
            btQuaternion quatYaw(btVector3(0, 1, 0), transform->rotation.y);   // Y-axis (yaw)
            btQuaternion quatRoll(btVector3(0, 0, 1), transform->rotation.z);  // Z-axis (roll)

            // Combine quaternions in the correct order: roll * yaw * pitch (ZYX)
            btQuaternion totalRotation = quatRoll * quatYaw * quatPitch;
            trans.setRotation(totalRotation);
            collision->bulletBody->getMotionState()->setWorldTransform(trans);
            collision->bulletBody->setWorldTransform(trans);
            collision->bulletBody->activate();
            physicsWorld->updateSingleAabb(collision->bulletBody);
        } else {
            // Update ECS from physics
            btTransform trans;
            collision->bulletBody->getMotionState()->getWorldTransform(trans);
            collision->bulletBody->activate();
            transform->position = glm::vec3(
                trans.getOrigin().x(),
                trans.getOrigin().y(),
                trans.getOrigin().z()
            );
            auto rotation = trans.getRotation();
            transform->rotation = glm::quat(
                rotation.w(),
                rotation.x(),
                rotation.y(),
                rotation.z()
            );
        }
    }

    void CollisionSystem::_clearPreviousCollisions(World* world) {
        for(auto entity : world->getEntities()) {
            if(auto collision = entity->getComponent<CollisionComponent>()) {
                collision->collidedEntities.clear();
            }
        }
    }

    void CollisionSystem::_detectCollisions() {
        btDispatcher* dispatcher = physicsWorld->getDispatcher();
        int numManifolds = dispatcher->getNumManifolds();

        for (int i = 0; i < numManifolds; i++) {
            btPersistentManifold* manifold = dispatcher->getManifoldByIndexInternal(i);
            const btCollisionObject* objA = static_cast<const btCollisionObject*>(manifold->getBody0());
            const btCollisionObject* objB = static_cast<const btCollisionObject*>(manifold->getBody1());

            // Only process pairs with active contacts
            if (manifold->getNumContacts() > 0) {
                Entity* entityA = static_cast<Entity*>(objA->getUserPointer());
                Entity* entityB = static_cast<Entity*>(objB->getUserPointer());

                if (entityA && entityB) {
                    // Update their collision components
                    CollisionComponent* collA = entityA->getComponent<CollisionComponent>();
                    CollisionComponent* collB = entityB->getComponent<CollisionComponent>();
                    if (collA) collA->collidedEntities.insert(entityB);
                    if (collB) collB->collidedEntities.insert(entityA);
                }
            }
        }
    }

    void CollisionSystem::update(World* world, float deltaTime) {
        _stepSimulation(deltaTime);
        _clearPreviousCollisions(world);
        _processEntities(world);
        _detectCollisions();
    }

    bool CollisionSystem::raycast(const glm::vec3& start, const glm::vec3& end, 
        CollisionComponent*& hitComponent, glm::vec3& hitPoint, glm::vec3& hitNormal) {
        if (!physicsWorld) return false;

        btVector3 btStart(start.x, start.y, start.z);
        btVector3 btEnd(end.x, end.y, end.z);

        // Configure raycast query
        btCollisionWorld::ClosestRayResultCallback rayCallback(btStart, btEnd);
        physicsWorld->rayTest(btStart, btEnd, rayCallback);

        if (rayCallback.hasHit()) {
            hitPoint = glm::vec3(
            rayCallback.m_hitPointWorld.x(),
            rayCallback.m_hitPointWorld.y(),
            rayCallback.m_hitPointWorld.z()
            );
            hitNormal = glm::vec3(
            rayCallback.m_hitNormalWorld.x(),
            rayCallback.m_hitNormalWorld.y(),
            rayCallback.m_hitNormalWorld.z()
            );
            // Extract the hit entity's collision component
            auto* hitBody = btRigidBody::upcast(rayCallback.m_collisionObject);
            if (hitBody && hitBody->getUserPointer()) {
                Entity* hitEntity = static_cast<Entity*>(hitBody->getUserPointer());
                hitComponent = hitEntity->getComponent<CollisionComponent>();
                return true;
            }
        }
        return false;
    }

    void CollisionSystem::applyImpulse(Entity* entity, const glm::vec3& force, const glm::vec3& position) {
        if (auto collision = entity->getComponent<CollisionComponent>()) {
            if (collision->bulletBody && collision->mass > 0.0f) {
                btVector3 btForce(force.x, force.y, force.z);
                btVector3 btPosition(position.x, position.y, position.z);
                collision->bulletBody->applyImpulse(btForce, btPosition);
                collision->bulletBody->activate();
            }
        }
    }
    
    void CollisionSystem::applyForce(Entity* entity, const glm::vec3& force, const glm::vec3& position) {
        if (auto collision = entity->getComponent<CollisionComponent>()) {
            if (collision->bulletBody && collision->mass > 0.0f) {
                btVector3 btForce(force.x, force.y, force.z);
                btVector3 btPosition(position.x, position.y, position.z);
                collision->bulletBody->applyForce(btForce, btPosition);
                collision->bulletBody->activate();
            }
        }
    }
    
    void CollisionSystem::applyTorque(Entity* entity, const glm::vec3& torque) {
        if (auto collision = entity->getComponent<CollisionComponent>()) {
            if (collision->bulletBody && collision->mass > 0.0f) {
                collision->bulletBody->applyTorque(btVector3(torque.x, torque.y, torque.z));
                collision->bulletBody->activate();
            }
        }
    }

    glm::vec3 CollisionSystem::_sweepTestGhostObject(btPairCachingGhostObject* ghost, const glm::vec3& movement) {
        const float MIN_MOVEMENT = 0.01f;
        const int MAX_ITERATIONS = 3;
        const float COLLISION_MARGIN = 0.0f;
        
        glm::vec3 remainingMovement = movement;
        glm::vec3 totalMovement(0.0f);
        int iterations = 0;

        btConvexShape* ghostShape = static_cast<btConvexShape*>(ghost->getCollisionShape());
        ghostShape->setMargin(COLLISION_MARGIN);
    
        while (iterations++ < MAX_ITERATIONS && glm::length(remainingMovement) > MIN_MOVEMENT) {
            btTransform start = ghost->getWorldTransform();
            btTransform end = start;
            end.setOrigin(start.getOrigin() + btVector3(remainingMovement.x, remainingMovement.y, remainingMovement.z));
    
            AllHitsConvexResultCallback allCb(
                start.getOrigin(), end.getOrigin()
            );
            allCb.m_collisionFilterGroup = btBroadphaseProxy::KinematicFilter;
            allCb.m_collisionFilterMask  = btBroadphaseProxy::StaticFilter | btBroadphaseProxy::KinematicFilter;
            
    
            btConvexShape* ghostShape = static_cast<btConvexShape*>(ghost->getCollisionShape());
            if (!ghostShape) {
                throw std::runtime_error("Ghost object has no collision shape!");
            }
    
            physicsWorld->convexSweepTest(ghostShape, start, end, allCb);
    
            bool collisionDetected = false;
            if (allCb.hasHit()) {
                const float EPS = 1e-3f;
                float bestFrac = 1.0f;
                int   bestIdx  = -1;

                for (int i = 0; i < allCb.hitFractions.size(); ++i) {
                    float f = allCb.hitFractions[i];
                    if (f > EPS && f < bestFrac) {
                        bestFrac = f;
                        bestIdx  = i;
                    }
                }
                if (bestIdx >= 0) {
                    // Use the best hit fraction to update the ghost position
                    btVector3 hitPointBt  = allCb.hitPointsWorld[bestIdx];
                    btVector3 hitNormalBt = allCb.hitNormalsWorld[bestIdx];

                    glm::vec3 hitNormal( hitNormalBt.x(), hitNormalBt.y(), hitNormalBt.z());
                    hitNormal = glm::normalize(hitNormal);

                    float bestFrac = allCb.hitFractions[bestIdx];
                    glm::vec3 allowedMovement = remainingMovement * bestFrac;
                    totalMovement += allowedMovement;

                    glm::vec3 remaining = remainingMovement - allowedMovement;
                    remainingMovement = remaining - hitNormal * glm::dot(remaining, hitNormal);
                    
                        // Update ghost position incrementally
                    btTransform newTransform = start;
                    newTransform.setOrigin(start.getOrigin() + btVector3(allowedMovement.x, allowedMovement.y, allowedMovement.z));
                    ghost->setWorldTransform(newTransform);
                    physicsWorld->updateSingleAabb(ghost);
                    collisionDetected = true;
                }
            } 
            if (!collisionDetected) {
                // No collision - apply full remaining movement
                totalMovement += remainingMovement;
                ghost->setWorldTransform(end);
                physicsWorld->updateSingleAabb(ghost);
                break;
            }
        }
    
        return totalMovement;
    }

    void CollisionSystem::_pushOverlappingObjects(btPairCachingGhostObject* ghost, const glm::vec3& movement) {
        ghost->getOverlappingPairCache()->cleanProxyFromPairs(
            ghost->getBroadphaseHandle(), physicsWorld->getDispatcher()
        );
        physicsWorld->performDiscreteCollisionDetection();
        const btBroadphasePairArray& pairs = ghost->getOverlappingPairCache()->getOverlappingPairArray();
        for (int i = 0; i < pairs.size(); ++i) {
            const btBroadphasePair& pair = pairs[i];
            btBroadphasePair* collisionPair = physicsWorld->getPairCache()->findPair(pair.m_pProxy0, pair.m_pProxy1);
            if (!collisionPair || !collisionPair->m_algorithm) continue;
    
            btManifoldArray manifolds;
            collisionPair->m_algorithm->getAllContactManifolds(manifolds);
    
            for (int j = 0; j < manifolds.size(); ++j) {
                btPersistentManifold* manifold = manifolds[j];
                const btCollisionObject* objA = static_cast<const btCollisionObject*>(manifold->getBody0());
                const btCollisionObject* objB = static_cast<const btCollisionObject*>(manifold->getBody1());
    
                const btCollisionObject* otherObj = (objA == ghost) ? objB : objA;
    
                if (btRigidBody* rb = btRigidBody::upcast(const_cast<btCollisionObject*>(otherObj))) {
                    // Get the name of the entity associated with the collision object
                    Entity* entity = static_cast<Entity*>(rb->getUserPointer());
                    // printf("Pushing entity: %s\n", entity->name.c_str());

                    if (rb->getInvMass() > 0.0f) {
                        btTransform dynTrans;
                        rb->getMotionState()->getWorldTransform(dynTrans);

                        btVector3 shift(movement.x, movement.y, movement.z);
                        dynTrans.setOrigin(dynTrans.getOrigin() + shift);

                        // Commit the new position
                        rb->getMotionState()->setWorldTransform(dynTrans);
                        rb->setWorldTransform(dynTrans);
                        rb->activate();
                    }
                }
            }
        }
    }

    void CollisionSystem::moveGhost(Entity* entity, const glm::vec3& movement) {
        CollisionComponent *collision = entity->getComponent<CollisionComponent>();
        if (!collision || !collision->ghostObject) return;
        btPairCachingGhostObject* ghost = collision->ghostObject;

        glm::vec3 actualMovement = _sweepTestGhostObject(ghost, movement);
        // glm::vec3 actualMovement = movement;
        const btTransform initialTransform = collision->ghostObject->getWorldTransform();
    
        try {
            glm::vec3 actualMovement = _sweepTestGhostObject(collision->ghostObject, movement);  // Currently useless
            _pushOverlappingObjects(collision->ghostObject, actualMovement);    // Currently buggy
        } catch (const std::exception& e) {
            // Reset to safe state on error
            collision->ghostObject->setWorldTransform(initialTransform);
            physicsWorld->updateSingleAabb(collision->ghostObject);
            std::cerr << "Ghost movement failed: " << e.what() << std::endl;
        }

        Transform transform;
        _syncTransforms(entity, collision, &transform);
        entity->localTransform.position = transform.position;
    }

    void CollisionSystem::debugDrawRay(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color) {
        if (debugDrawer) {
            debugDrawer->drawLine(
                btVector3(start.x, start.y, start.z),
                btVector3(end.x, end.y, end.z),
                btVector3(color.r, color.g, color.b)
            );
        }
    }

    void CollisionSystem::debugDrawWorld(World *world) {
        if (!debugDrawer || !physicsWorld) return;
        
        // Manually iterate through all collision objects
        auto& collisionObjects = physicsWorld->getCollisionObjectArray();
        for (int i = 0; i < collisionObjects.size(); ++i) {
            btCollisionObject* obj = collisionObjects[i];

            // Set color based on collision flags
            btVector3 color(1, 1, 1);
            if (obj->getCollisionFlags() & btCollisionObject::CF_KINEMATIC_OBJECT) {
                color = btVector3(1, 1, 0);
            } else if (obj->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT) {
                color = btVector3(0, 1, 0);
            } else if (obj->getCollisionFlags() & btCollisionObject::CF_DYNAMIC_OBJECT) {
                color = btVector3(0, 0, 1);
            } else if (auto entity = static_cast<Entity*>(obj->getUserPointer())) {
                auto collision = entity->getComponent<CollisionComponent>();
                if (collision && !collision->collidedEntities.empty()) {
                    color = btVector3(1, 0, 0);
                }
                if (collision->ghostObject) {
                    physicsWorld->debugDrawObject(
                        collision->ghostObject->getWorldTransform(),
                        collision->ghostObject->getCollisionShape(),
                        btVector3(1, 0.5, 0) // Orange color for ghost
                    );
                }
            }

            physicsWorld->debugDrawObject(
                obj->getWorldTransform(),
                obj->getCollisionShape(),
                color
            );
        }
        debugDrawer->flushLines(world);
    }

    void CollisionSystem::destroy() {
        if (debugDrawer) {
            delete debugDrawer;
            debugDrawer = nullptr;
        }

        if (physicsWorld) {
            delete physicsWorld;
            physicsWorld = nullptr;
        }
    }

}