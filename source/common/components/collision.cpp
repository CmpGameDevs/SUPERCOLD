#include <iostream>
#include <deserialize-utils.hpp>
#include <asset-loader.hpp>
#include <mesh/mesh.hpp>
#include "collision.hpp"

namespace our {
    
    CollisionComponent::~CollisionComponent() {
        if(bulletBody) {
            delete bulletBody->getCollisionShape();
            delete bulletBody->getMotionState();
            delete bulletBody;
        }

        if (triangleMesh) {
            delete triangleMesh;
        }

        if (ghostObject) {
            delete ghostObject->getCollisionShape();
            delete ghostObject;
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
    }
    
}