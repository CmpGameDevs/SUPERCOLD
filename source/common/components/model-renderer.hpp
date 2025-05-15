#pragma once

#include "../ecs/component.hpp"
#include "../model/model.hpp"
#include "../asset-loader.hpp"

namespace our {

    // This component denotes that any renderer should draw the given model
    class ModelComponent : public Component {
    public:
        Model* model;

        // The ID of this component type is "Mesh Renderer"
        static std::string getID() { return "Model Renderer"; }

        // Receives the mesh & material from the AssetLoader by the names given in the json object
        void deserialize(const nlohmann::json& data) override;
    };

}
