#include "collision-system.hpp"
#include "../ecs/transform.hpp"
#include <BulletCollision/CollisionShapes/btShapeHull.h>

namespace our {

    void CollisionSystem::initialize(glm::ivec2 windowSize, btDynamicsWorld* physicsWorld) {
        this->physicsWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld);
        this->debugDrawer = new GLDebugDrawer(windowSize);
        this->physicsWorld->setDebugDrawer(debugDrawer);
        if (this->physicsWorld->getDebugDrawer())
            this->physicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_DrawWireframe);
        this->physicsWorld->setGravity(btVector3(0, -9.81f, 0));
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
            Transform* transform = &entity->localTransform;
            
            if(collision && transform) {
                if(!collision->bulletBody) {
                    _createRigidBody(entity, collision, transform);
                }
                _syncTransforms(entity, collision, transform);
            }
        }
    }

    void CollisionSystem::_createRigidBody(Entity* entity, CollisionComponent* collision, Transform* transform) {
        btCollisionShape* shape = nullptr;
                    
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
            case CollisionShape::MESH: {
                if (!collision->triangleMesh) {
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
                }
        
                if (collision->mass <= 0.0f) {
                    shape = new btBvhTriangleMeshShape(collision->triangleMesh, true, true);
                } else {
                    // For dynamic objects, create convex hull
                    auto* convexRaw = new btConvexHullShape(
                        (btScalar*)&collision->vertices[0].position,
                        (int)collision->vertices.size(),
                        sizeof(our::Vertex)
                    );
                    convexRaw->setMargin(0.1f);
                    
                    // Simplify using btShapeHull
                    btShapeHull* hull = new btShapeHull(convexRaw);
                    hull->buildHull(convexRaw->getMargin());
                    
                    btConvexHullShape* simplified = new btConvexHullShape();
                    for (int i = 0; i < hull->numVertices(); ++i) {
                        simplified->addPoint(hull->getVertexPointer()[i]);
                    }
                    
                    delete convexRaw;
                    delete hull;
                    shape = simplified;
                }
                break;
            }
        }
    
        btTransform btTrans;
        btTrans.setIdentity();
        btTrans.setOrigin(btVector3(
            transform->position.x,
            transform->position.y,
            transform->position.z
        ));
        btVector3 inertia(0, 0, 0);
        if(collision->mass > 0 && !collision->isKinematic)
            shape->calculateLocalInertia(collision->mass, inertia);
        
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
        physicsWorld->addRigidBody(collision->bulletBody);
    }

    void CollisionSystem::_syncTransforms(Entity* entity, CollisionComponent* collision, Transform* transform) {
        // Skip static objects
        if (collision->mass == 0.0f && !collision->isKinematic) return;

        if(collision->isKinematic) {
            // Update physics from ECS
            btTransform trans;
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
            collision->bulletBody->setWorldTransform(trans);
            collision->bulletBody->activate();
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
            auto quat = glm::quat(
                rotation.w(),
                rotation.x(),
                rotation.y(),
                rotation.z()
            );
            // Convert quaternion to Euler angles (pitch, yaw, roll)
            transform->rotation = glm::eulerAngles(quat);
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
            if (obj->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT) {
                color = btVector3(0, 1, 0);
            } else if (obj->getCollisionFlags() & btCollisionObject::CF_KINEMATIC_OBJECT) {
                color = btVector3(1, 1, 0);
            } else if (obj->getCollisionFlags() & btCollisionObject::CF_DYNAMIC_OBJECT) {
                color = btVector3(0, 0, 1);
            } else if (auto entity = static_cast<Entity*>(obj->getUserPointer())) {
                auto collision = entity->getComponent<CollisionComponent>();
                if (collision && !collision->collidedEntities.empty()) {
                    color = btVector3(1, 0, 0);
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