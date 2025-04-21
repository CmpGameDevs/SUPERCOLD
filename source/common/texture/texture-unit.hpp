#pragma once

#include <glad/gl.h>
#include <json/json.hpp>
#include <glm/vec4.hpp>

namespace our {

    class TextureUnits{
    public:
        static const int TEXTURE_UNIT_ALBEDO = 1;
        static const int TEXTURE_UNIT_METALLIC = 2;
        static const int TEXTURE_UNIT_ROUGHNESS = 3;
        static const int TEXTURE_UNIT_NORMAL = 4;
        static const int TEXTURE_UNIT_AMBIENT_OCCLUSION = 5;
        static const int TEXTURE_UNIT_EMISSIVE = 6;
        static const int TEXTURE_UNIT_IRRADIANCE = 7;
        static const int TEXTURE_UNIT_ENVIRONMENT = 8;
        static const int TEXTURE_UNIT_HDR = 9;
        static const int TEXTURE_UNIT_BRDF = 10;
        static const int TEXTURE_UNIT_PREFILTER = 11;
        static const int TEXTURE_UNIT_METALLIC_ROUGHNESS = 12;
    };

}