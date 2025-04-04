#version 330 core
precision highp float;
#define PI 3.1415926535897932384626433832795
#define GREYSCALE_WEIGHT_VECTOR vec3(0.2126, 0.7152, 0.0722)

layout (location = 0) out vec4 FragColor;

in vec3 worldCoordinates;
in vec2 textureCoordinates;
in vec3 tangent;
in vec3 bitangent;
in vec3 normal;

struct Material {
    bool useTextureAlbedo;
    bool useTextureMetallicRoughness;
    bool useTextureNormal;
    bool useTextureAmbientOcclusion;
    bool useTextureEmissive;

    vec3 albedo;
    float metallic;
    float roughness;
    float ambientOcclusion;
    vec3 emissive;

    sampler2D textureAlbedo;
    sampler2D textureMetallicRoughness;
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
uniform Light lights[1];

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a2 = roughness * roughness * roughness * roughness;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    return geometrySchlickGGX(max(dot(N, V), 0.0), roughness) *
           geometrySchlickGGX(max(dot(N, L), 0.0), roughness);
}

vec3 calculateNormal(vec3 tangentNormal) {
    vec3 norm = normalize(tangentNormal * 2.0 - 1.0);
    mat3 TBN = mat3(tangent, bitangent, normal);
    return normalize(TBN * norm);
}

void main() {
    vec3 albedo = material.albedo;
    if (material.useTextureAlbedo) {
        albedo = texture(material.textureAlbedo, textureCoordinates).rgb;
    }

    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.useTextureMetallicRoughness) {
        vec3 metallicRoughness = texture(material.textureMetallicRoughness, textureCoordinates).rgb;
        metallic = metallicRoughness.b;
        roughness = metallicRoughness.g;
    }

    vec3 N = normal;
    if (material.useTextureNormal) {
        N = calculateNormal(texture(material.textureNormal, textureCoordinates).rgb);
    }

    float ao = material.ambientOcclusion;
    if (material.useTextureAmbientOcclusion) {
        ao = texture(material.textureAmbientOcclusion, textureCoordinates).r;
    }

    vec3 emissive = material.emissive;
    if (material.useTextureEmissive) {
        emissive = texture(material.textureEmissive, textureCoordinates).rgb;
    }

    vec3 V = normalize(cameraPosition - worldCoordinates);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
	int i = 0;
    // for (int i = 0; i < 1; i++) {
        vec3 L = normalize(lights[i].position - worldCoordinates);
        vec3 H = normalize(V + L);

        float distance = length(lights[i].position - worldCoordinates);
        float attenuation = 1.0 / (lights[i].attenuation_constant +
                                   lights[i].attenuation_linear * distance +
                                   lights[i].attenuation_quadratic * distance * distance);
        vec3 radiance = lights[i].color * attenuation;

        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        vec3 specular = numerator / max(denominator, 0.001);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 diffuse = kD * albedo / PI;
        float NdotL = max(dot(N, L), 0.0);

        Lo += (diffuse + specular) * radiance * NdotL;
    // }

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = Lo + ambient + emissive;

    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}