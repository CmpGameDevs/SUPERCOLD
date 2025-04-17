#include "collision-system.hpp"
#include "../components/collision.hpp"
#include "../ecs/transform.hpp"

namespace our {

    void CollisionSystem::initialize(glm::ivec2 windowSize, btDynamicsWorld* physicsWorld) {
        this->physicsWorld = static_cast<btDiscreteDynamicsWorld*>(physicsWorld);
        this->debugDrawer = new GLDebugDrawer(windowSize);
        this->physicsWorld->setDebugDrawer(debugDrawer);
        if (this->physicsWorld->getDebugDrawer())
            this->physicsWorld->getDebugDrawer()->setDebugMode(btIDebugDraw::DBG_DrawWireframe);
        this->physicsWorld->setGravity(btVector3(0, -9.81f, 0));
    }

    void CollisionSystem::stepSimulation(float deltaTime) {
        if (physicsWorld) {
            physicsWorld->stepSimulation(deltaTime);
        } else {
            printf("[ERROR] CollisionSystem::stepSimulation: physicsWorld is null!\n");
        }
    }

    void CollisionSystem::update(World* world, float deltaTime) {
        // Store entities
        std::unordered_set<Entity*> entities = world->getEntities();
        for(auto entity : entities) {
            auto collision = entity->getComponent<CollisionComponent>();
            Transform* transform = &entity->localTransform;
            
            if(collision && transform) {
                // Create bullet body if not exists
                if(!collision->bulletBody) {
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
                    }

                    // Create rigid body
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
                    else if (collision->isKinematic)
                        collision->mass = 0.0f;
                    
                    btMotionState* motionState = new btDefaultMotionState(btTrans);
                    btRigidBody::btRigidBodyConstructionInfo rbInfo(
                        collision->mass, 
                        motionState,
                        shape,
                        inertia
                    );
                    collision->bulletBody = new btRigidBody(rbInfo);
                    collision->bulletBody->setUserPointer(entity);

                    // Add a damping to the body if it has mass to simulate air resistance
                    if (collision->mass) {
                        btScalar linearDamping = 0.1f / collision->mass; 
                        btScalar angularDamping = 0.05f / collision->mass;
    
                        collision->bulletBody->setDamping(linearDamping, angularDamping);
                    }
                    physicsWorld->addRigidBody(collision->bulletBody);
                }
    
                // Sync transforms
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
        }
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