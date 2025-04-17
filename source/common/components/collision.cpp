// source/common/components/collision.cpp
#include "collision.hpp"
#include "../deserialize-utils.hpp"

namespace our {
    
    CollisionComponent::~CollisionComponent() {
        if(bulletBody) {
            delete bulletBody->getCollisionShape();
            delete bulletBody->getMotionState();
            delete bulletBody;
        }
    }
    
    void CollisionComponent::deserialize(const nlohmann::json& data) {
        // Shape parsing
        if(data.contains("shape")) {
            std::string shapeStr = data["shape"];
            if(shapeStr == "box") shape = CollisionShape::BOX;
            else if(shapeStr == "sphere") shape = CollisionShape::SPHERE;
            else if(shapeStr == "capsule") shape = CollisionShape::CAPSULE;
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