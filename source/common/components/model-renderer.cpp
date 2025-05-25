#include "model-renderer.hpp"
#include "../asset-loader.hpp"

namespace our {
    void ModelComponent::deserialize(const nlohmann::json& data){
        if(!data.is_object()) return;

        model = AssetLoader<Model>::get(data["model"].get<std::string>());
    }
}
