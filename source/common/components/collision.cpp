#include <iostream>
#include <deserialize-utils.hpp>
#include <asset-loader.hpp>
#include <mesh/mesh.hpp>
#include <model/model.hpp>
#include <systems/collision-system.hpp>
#include "collision.hpp"

namespace our {
    
    CollisionComponent::~CollisionComponent() {
        freeBulletBody();
        freeGhostObject();

        if (triangleMesh) {
            delete triangleMesh;
        }
    }
    
    void CollisionComponent::freeGhostObject() {
        if (ghostObject) {
            CollisionSystem::getInstance().getPhysicsWorld()->removeCollisionObject(ghostObject);
            btCollisionShape* shape = ghostObject->getCollisionShape();
            if (shape) {
                delete shape;
            }
            delete ghostObject;
        }
    }

    void CollisionComponent::freeBulletBody() {
        if (bulletBody) {
            // Remove the body from the physics world if it exists
            CollisionSystem::getInstance().getPhysicsWorld()->removeCollisionObject(bulletBody);

            // Delete the motion state and collision shape
            delete bulletBody->getCollisionShape();
            delete bulletBody->getMotionState();
            delete bulletBody;
            bulletBody = nullptr;
        }
    }

    void CollisionComponent::deserialize(const nlohmann::json& data) {
        // Shape parsing
        if(data.contains("shape")) {
            std::string shapeStr = data["shape"];
            if(shapeStr == "box") {
                shape = CollisionShape::BOX;
                dragCoefficient = 1.05f;
                crossSectionArea = halfExtents.x * halfExtents.y * 2.0f + halfExtents.x * halfExtents.z * 2.0f + halfExtents.y * halfExtents.z * 2.0f;
            } else if(shapeStr == "sphere") {
                shape = CollisionShape::SPHERE;
                dragCoefficient = 0.47f;
                crossSectionArea = glm::pi<float>() * halfExtents.x * halfExtents.x;
            } else if(shapeStr == "capsule") {
                shape = CollisionShape::CAPSULE;
                dragCoefficient = 0.82f;
                crossSectionArea = glm::pi<float>() * halfExtents.x * halfExtents.x;
            }
            else if(shapeStr == "mesh") {
                shape = CollisionShape::MESH;
                if (data.contains("mesh")) {
                    Mesh* mesh = AssetLoader<Mesh>::get(data["mesh"].get<std::string>());
                    vertices = mesh->cpuVertices;
                    indices = mesh->cpuIndices;
                }
                if(data.contains("vertices")) {
                    for(auto& v : data["vertices"]) {
                        vertices.push_back({
                            glm::vec3(v["position"][0], v["position"][1], v["position"][2]),
                            our::Color{},  // Color not needed for collision
                            glm::vec2{},   // Tex coord not needed
                            glm::vec3{}    // Normal not needed
                        });
                    }
                }
                if(data.contains("indices")) {
                    indices = data["indices"].get<std::vector<uint32_t>>();
                }
            }
            else if(shapeStr == "model") {
                shape = CollisionShape::COMPOUND;
                loadModel(data["model"].get<std::string>());
            }
            else if(shapeStr == "ghost") shape = CollisionShape::GHOST;
            else {
                std::cerr << "Unknown collision shape: " << shapeStr << std::endl;
                shape = CollisionShape::BOX; // Default to box if unknown
            }
        }
        
        // Physics properties
        mass = data.value("mass", 0.0f);
        isKinematic = data.value("isKinematic", false);
        
        // Dimensions
        if(data.contains("halfExtents")) {
            halfExtents.x = data["halfExtents"][0].get<float>();
            halfExtents.y = data["halfExtents"][1].get<float>();
            halfExtents.z = data["halfExtents"][2].get<float>();
        }

        if (data.contains("centerOffset")) {
            centerOffset.x = data["centerOffset"][0].get<float>();
            centerOffset.y = data["centerOffset"][1].get<float>();
            centerOffset.z = data["centerOffset"][2].get<float>();
        }
    }

    void CollisionComponent::loadModel(const std::string& modelPath) {
        Model* model = AssetLoader<Model>::get(modelPath);
        // Add each mesh as a child shape
        for(unsigned int i = 0; i < model->meshRenderers.size(); i++) {
            Mesh* mesh = model->meshRenderers[i]->mesh;
            glm::mat4 meshWorldMatrix = model->matricesMeshes[i];
            
            CollisionComponent::ChildShape childShape;
            childShape.shape = CollisionShape::MESH;
            childShape.vertices.reserve(mesh->cpuVertices.size());
            
            // Transform vertices to mesh-local space
            for(const auto& vertex : mesh->cpuVertices) {
                Vertex transformedVertex = vertex;
                glm::vec4 transformedPos = meshWorldMatrix * glm::vec4(vertex.position, 1.0f);
                transformedVertex.position = glm::vec3(transformedPos);
                childShape.vertices.push_back(transformedVertex);
            }
            
            childShape.indices = mesh->cpuIndices;
            childShapes.push_back(childShape);
        }
    }
    
}