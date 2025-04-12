#version 330 core
precision highp float;
#define PI 3.1415926535897932384626433832795
#define MAX_LIGHTS 4
#define GREYSCALE_WEIGHT_VECTOR vec3(0.2126, 0.7152, 0.0722)

layout (location = 0) out vec4 FragColor;

in vec3 worldCoordinates;
in vec2 textureCoordinates;
in vec3 tangent;
in vec3 bitangent;
in vec3 normal;

struct Material {
    bool useTextureAlbedo;
    bool useTextureMetallic;
    bool useTextureRoughness;
    bool useTextureNormal;
    bool useTextureAmbientOcclusion;
    bool useTextureEmissive;

    vec3 albedo;
    float metallic;
    float roughness;
    float ambientOcclusion;
    vec3 emissive;

    sampler2D textureAlbedo;
    sampler2D textureMetallic;
    sampler2D textureRoughness;
    sampler2D textureNormal;
    sampler2D textureAmbientOcclusion;
    sampler2D textureEmissive;
};

struct Light {
    vec3 position;
    vec3 color;
    float attenuation_constant;
    float attenuation_linear;
    float attenuation_quadratic;
};

uniform Material material;
uniform vec3 cameraPosition;
uniform Light lights[MAX_LIGHTS];
uniform int lightCount;


// Fresnel function (Fresnel-Schlick approximation)
//
// F_schlick = f0 + (1 - f0)(1 - (h * v))^5
//
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

// Normal distribution function (Trowbridge-Reitz GGX)
//
//                alpha ^ 2
//     ---------------------------------
//      PI((n * h)^2(alpha^2 - 1) + 1)^2
//
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a2 = roughness * roughness * roughness * roughness;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

// Geometry function
//
//         n * v
//   -------------------
//   (n * v)(1 - k) + k
//
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

// smiths method for taking into account view direction and light direction
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

// ----------------------------------------------------------------------------
// Easy trick to get tangent-normals to world-space to keep PBR code simplified.
// Don't worry if you don't get what's going on; you generally want to do normal 
// mapping the usual way for performance anyways; I do plan make a note of this 
// technique somewhere later in the normal mapping tutorial.
vec3 getNormalFromMap()
{
    vec3 tangentNormal = texture(material.textureNormal, textureCoordinates).xyz * 2.0 - 1.0;

    vec3 Q1  = dFdx(worldCoordinates);
    vec3 Q2  = dFdy(worldCoordinates);
    vec2 st1 = dFdx(textureCoordinates);
    vec2 st2 = dFdy(textureCoordinates);

    vec3 N   = normalize(normal);
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main() {
    // retrieve all the material properties

    // albedo
    vec3 albedo = material.albedo;
    if (material.useTextureAlbedo) {
        albedo = pow(texture(material.textureAlbedo, textureCoordinates).rgb, vec3(2.2));
    }

    // metallic/roughness
    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.useTextureMetallic) {
        metallic = texture(material.textureMetallic, textureCoordinates).r;
    }

    if (material.useTextureRoughness) {
        roughness = texture(material.textureRoughness, textureCoordinates).r;
    }

    // normal
    vec3 N = normalize(normal);
    if (material.useTextureNormal) {
        N = getNormalFromMap();
    }

    // ambient occlusion
    float ao = material.ambientOcclusion;
    if (material.useTextureAmbientOcclusion) {
        ao = texture(material.textureAmbientOcclusion, textureCoordinates).r;
    }

    // emissive
    vec3 emissive = material.emissive;
    if (material.useTextureEmissive) {
        emissive = texture(material.textureEmissive, textureCoordinates).rgb;
    }

    vec3 V = normalize(cameraPosition - worldCoordinates); // view vector pointing at camera
    // f0 is the "surface reflection at zero incidence"
    // for PBR-metallic we assume dialectrics all have 0.04
	// for metals the value comes from the albedo map
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);  // total radiance out

    // Direct lighting
	// Sum up the radiance contributions of each light source.
	// This loop is essentially the integral of the rendering equation.
    for (int i = 0; i < lightCount; i++) {
        vec3 L = normalize(lights[i].position - worldCoordinates);
        vec3 H = normalize(V + L);

        float distance = length(lights[i].position - worldCoordinates);
        float attenuation = 1.0 / (distance * distance);
        vec3 radiance = lights[i].color * attenuation;

        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 diffuse = kD * albedo / PI;
        float NdotL = max(dot(N, L), 0.0);

        Lo += (diffuse + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = Lo + ambient + emissive;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}