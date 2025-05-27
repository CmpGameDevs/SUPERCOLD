#include "model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>
#include <asset-loader.hpp>
#include <ecs/entity.hpp>
#include <iomanip>
#include <settings.hpp>
#include "animation/animation.hpp"
#include "glad/gl.h"
#include "glm/common.hpp"
#include "material/material.hpp"
#include "texture/texture-utils.hpp"
#include "texture/texture2d.hpp"

namespace our {

template <typename TKey> static unsigned int findKeyIndex(float animationTime, const TKey* keys, unsigned int numKeys) {
    if (numKeys <= 1)
        return 0;

    for (unsigned int i = 0; i < numKeys - 1; ++i) {
        if (animationTime < keys[i + 1].mTime) {
            return i;
        }
    }
    return numKeys - 1;
}

static glm::vec3 interpolatePosition(float animationTime, const aiVectorKey* keys, unsigned int numKeys) {
    if (numKeys == 0)
        return glm::vec3(0.0f);
    if (numKeys == 1)
        return Model::aiToGlm(keys[0].mValue);

    unsigned int keyIndex = findKeyIndex(animationTime, keys, numKeys);
    unsigned int nextKeyIndex = keyIndex + 1;

    if (nextKeyIndex >= numKeys) {
        return Model::aiToGlm(keys[numKeys - 1].mValue);
    }

    const aiVectorKey& key1 = keys[keyIndex];
    const aiVectorKey& key2 = keys[nextKeyIndex];

    float deltaTime = static_cast<float>(key2.mTime - key1.mTime);
    float factor = 0.0f;
    if (deltaTime > 0.00001f) {
        factor = static_cast<float>(animationTime - key1.mTime) / deltaTime;
    }

    return glm::mix(Model::aiToGlm(key1.mValue), Model::aiToGlm(key2.mValue), factor);
}

static glm::quat interpolateRotation(float animationTime, const aiQuatKey* keys, unsigned int numKeys) {
    if (numKeys == 0)
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (numKeys == 1)
        return Model::aiToGlm(keys[0].mValue);

    unsigned int keyIndex = findKeyIndex(animationTime, keys, numKeys);
    unsigned int nextKeyIndex = keyIndex + 1;

    if (nextKeyIndex >= numKeys) {
        return Model::aiToGlm(keys[numKeys - 1].mValue);
    }

    const aiQuatKey& key1 = keys[keyIndex];
    const aiQuatKey& key2 = keys[nextKeyIndex];

    float deltaTime = static_cast<float>(key2.mTime - key1.mTime);
    float factor = 0.0f;
    if (deltaTime > 0.00001f) {
        factor = static_cast<float>(animationTime - key1.mTime) / deltaTime;
    }

    return glm::slerp(Model::aiToGlm(key1.mValue), Model::aiToGlm(key2.mValue), factor);
}

static glm::vec3 interpolateScale(float animationTime, const aiVectorKey* keys, unsigned int numKeys) {
    if (numKeys == 0)
        return glm::vec3(1.0f);
    if (numKeys == 1)
        return Model::aiToGlm(keys[0].mValue);

    unsigned int keyIndex = findKeyIndex(animationTime, keys, numKeys);
    unsigned int nextKeyIndex = keyIndex + 1;

    if (nextKeyIndex >= numKeys) {
        return Model::aiToGlm(keys[numKeys - 1].mValue);
    }

    const aiVectorKey& key1 = keys[keyIndex];
    const aiVectorKey& key2 = keys[nextKeyIndex];

    float deltaTime = static_cast<float>(key2.mTime - key1.mTime);
    float factor = 0.0f;
    if (deltaTime > 0.00001f) {
        factor = static_cast<float>(animationTime - key1.mTime) / deltaTime;
    }

    return glm::mix(Model::aiToGlm(key1.mValue), Model::aiToGlm(key2.mValue), factor);
}

Model::~Model() {
    for (auto* mr : meshRenderers)
        delete mr;
}

bool Model::loadFromFile(const std::string& path) {
    std::cout << "\x1b[32m" << std::string(120, '=') << "\x1b[0m" << std::endl;

    std::cout << "[Model] Loading model from: " << path << std::endl;

    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        path, aiProcess_Triangulate | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals |
                  aiProcess_JoinIdenticalVertices | aiProcess_ImproveCacheLocality | aiProcess_OptimizeMeshes |
                  aiProcess_OptimizeGraph | aiProcess_SortByPType | aiProcess_PopulateArmatureData |
                  aiProcess_GenUVCoords | aiProcess_TransformUVCoords);

    if (!scene || !scene->mRootNode && (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE)) {
        std::cerr << "[Model] ERROR: Failed to load model: " << path << " (" << importer.GetErrorString() << ")"
                  << std::endl;
        return false;
    }

    std::cout << "[Model] Successfully loaded scene with " << scene->mNumMeshes << " meshes and "
              << scene->mNumMaterials << " materials, and " << scene->mNumTextures << " embedded textures."
              << std::endl;

    directory = path.substr(0, path.find_last_of("/\\") + 1);

    if (scene->HasAnimations() || scene->mNumMeshes > 0) {
        bool hasAnyBones = false;
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
            if (scene->mMeshes[i]->HasBones()) {
                hasAnyBones = true;
                break;
            }
        }
        if (hasAnyBones) {
            loadSkeletonFromScene(scene);
        } else {
            std::cout << "[Model] No bones found in any mesh, skipping skeleton loading." << std::endl;
        }
    }

    // Load Animations
    if (scene->HasAnimations()) {
        loadAnimationsFromScene(scene);
    } else {
        std::cout << "[Model] Scene contains no animations to load." << std::endl;
    }

    loadMaterialsFromScene(scene);

    processNode(scene->mRootNode, scene, glm::mat4(1.0f));

    generateCombinedMesh();
    std::cout << "[Model] Generated combined mesh with " << (combinedMesh ? combinedMesh->cpuVertices.size() : 0)
              << " vertices." << std::endl;

    std::cout << "\x1b[32m" << std::string(120, '=') << "\x1b[0m" << std::endl;
    return true;
}

void Model::processNode(const aiNode* node, const aiScene* scene, const glm::mat4& parentTransform) {
    glm::mat4 nodeTransform = parentTransform * aiToGlm(node->mTransformation);

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshRenderers.push_back(processMesh(mesh, scene, nodeTransform));
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        processNode(node->mChildren[i], scene, nodeTransform);
    }
}

MeshRendererComponent* Model::processMesh(const aiMesh* mesh, const aiScene* scene, const glm::mat4& transform) {
    std::vector<Vertex> verts;
    std::vector<unsigned int> inds;
    verts.reserve(mesh->mNumVertices);

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        Vertex v;

        v.position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);

        if (mesh->mNormals)
            v.normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);

        if (mesh->mTextureCoords[0])
            v.tex_coord = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);

        if (mesh->mColors[0])
            v.color = Color(mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a);

        // bone data
        for (int j = 0; j < MAX_BONE_INFLUENCE; ++j) {
            v.bone_ids[j] = -1;
            v.weights[j] = 0.0f;
        }

        verts.push_back(v);
    }

    processVertexBoneData(mesh, verts);

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        const aiFace& face = mesh->mFaces[f];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
            inds.push_back(face.mIndices[j]);
    }

    auto* m = new Mesh(verts, inds);
    Material* mat = nullptr;

    if (mesh->mMaterialIndex >= 0 && static_cast<size_t>(mesh->mMaterialIndex) < materials.size()) {
        mat = materials[mesh->mMaterialIndex].get();
    }

    auto* mr = new MeshRendererComponent();
    mr->mesh = m;
    mr->material = mat;
    mr->localToParent = transform;
    return mr;
}

void Model::generateCombinedMesh() {
    if (meshRenderers.empty())
        return;

    std::vector<Vertex> verts;
    std::vector<unsigned int> inds;
    unsigned int offset = 0;

    for (auto* mr : meshRenderers) {

        for (const auto& v : mr->mesh->cpuVertices) {
            Vertex tv = v;
            glm::vec4 p = mr->localToParent * glm::vec4(tv.position, 1.0f);
            tv.position = glm::vec3(p);
            glm::mat3 nm = glm::transpose(glm::inverse(glm::mat3(mr->localToParent)));
            tv.normal = glm::normalize(nm * tv.normal);
            verts.push_back(tv);
        }

        for (auto i : mr->mesh->cpuIndices)
            inds.push_back(i + offset);
        offset += mr->mesh->cpuVertices.size();
    }
    combinedMesh = std::make_unique<Mesh>(verts, inds);
}

void Model::draw(CameraComponent* camera, const glm::mat4& localToWorld, const glm::ivec2& windowSize,
                 float bloomCutoff) const {
    if (!camera || !camera->getOwner()) {
        std::cerr << "[Model] ERROR: Camera or camera owner is null in draw call." << std::endl;
        return;
    }

    Settings& settings = Settings::getInstance();

    // Calculate View and Projection matrices once
    glm::mat4 projectionMatrix = camera->getProjectionMatrix(windowSize);
    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix; // VP

    // Get camera world position
    glm::vec3 cameraWorldPosition = glm::vec3(camera->getOwner()->getLocalToWorldMatrix()[3]);

    for (const auto& meshRendererUniquePtr : meshRenderers) {
        const MeshRendererComponent* meshRenderer = meshRendererUniquePtr;

        if (!meshRenderer || !meshRenderer->material || !meshRenderer->mesh || !meshRenderer->material->shader) {
            if (!meshRenderer)
                std::cerr << "[Model] WARNING: Skipping draw due to null "
                             "meshRenderer."
                          << std::endl;
            else if (!meshRenderer->material)
                std::cerr << "[Model] WARNING: Skipping draw due to null material." << std::endl;
            else if (!meshRenderer->mesh)
                std::cerr << "[Model] WARNING: Skipping draw due to null mesh." << std::endl;
            else if (!meshRenderer->material->shader)
                std::cerr << "[Model] WARNING: Skipping draw due to null "
                             "shader on material."
                          << std::endl;
            continue;
        }

        meshRenderer->material->setup();

        // Calculate Model matrix for this specific mesh component
        // mr->localToParent transforms the mesh from its local space to the
        // model's root local space. localToWorld transforms the model's root
        // local space to world space.
        glm::mat4 modelMatrix = localToWorld * meshRenderer->localToParent; // M

        // Calculate other derived matrices
        glm::mat4 modelViewMatrix = viewMatrix * modelMatrix;                     // MV
        glm::mat4 modelViewProjectionMatrix = viewProjectionMatrix * modelMatrix; // MVP

        // Set shader uniforms
        meshRenderer->material->shader->set("cameraPosition", camera->getOwner()->localTransform.position);

        // Standard matrix uniforms
        meshRenderer->material->shader->set("model", modelMatrix);
        meshRenderer->material->shader->set("view", viewMatrix);
        meshRenderer->material->shader->set("projection", projectionMatrix);
        meshRenderer->material->shader->set("MV", modelViewMatrix);
        meshRenderer->material->shader->set("VP", viewProjectionMatrix);
        meshRenderer->material->shader->set("transform", modelViewProjectionMatrix);

        meshRenderer->material->shader->set("cameraPosition", cameraWorldPosition);

        meshRenderer->material->shader->set("bloomBrightnessCutoff", bloomCutoff);

        // set bone transforms if skeleton is present
        if (skeleton.getBoneCount() > 0) {

            auto& boneTransforms = skeleton.getFinalTransforms();
            for (unsigned int i = 0; i < boneTransforms.size(); ++i) {
                meshRenderer->material->shader->set("boneFinalTransforms[" + std::to_string(i) + "]",
                                                    boneTransforms[i]);
            }
        }

        meshRenderer->material->shader->set("debugMode", settings.shaderDebugModeToInt(settings.shaderDebugMode));

        if (settings.shaderDebugMode == "wireframe") {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        meshRenderer->mesh->draw();
    }
}

void Model::loadMaterialsFromScene(const aiScene* scene) {
    materials.reserve(scene->mNumMaterials);

    std::cout << "[Model] Loading " << scene->mNumMaterials << " materials..." << std::endl;

    for (unsigned int i = 0; i < scene->mNumMaterials; ++i) {
        materials.push_back(processMaterial(scene->mMaterials[i], scene));
    }
}

std::unique_ptr<Material> Model::processMaterial(const aiMaterial* aiMat, const aiScene* scene) {
    // TODO: add logic here to choose different material types based on
    // aiMaterial properties. after implementing these types of material.
    auto material = std::make_unique<LitMaterial>();

    material->transparent = false;
    material->metallic = 0.95f;
    material->roughness = 0.1f;

    aiString matName;
    if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
        std::cout << "[Model] Processing material: " << aiToStr(matName) << std::endl;
    }

    material->shader = AssetLoader<ShaderProgram>::get("pbr");

    if (!material->shader) {
        std::cerr << "[Model] WARNING: Default PBR shader not found for material: " << aiToStr(matName) << std::endl;
        return material;
    }

    // --- Set Material Properties ---
    // Colors and Factors
    aiColor4D color(0.0f, 0.0f, 0.0f, 1.0f);
    float factor = 1.0f;

    if (aiMat->Get(AI_MATKEY_BASE_COLOR, color) == AI_SUCCESS) {
        material->albedo = glm::vec3(color.r, color.g, color.b);
        material->tint.a = color.a;                                        // Use base color's alpha for tint's alpha
    } else if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) { // Fallback for non-PBR
        material->albedo = glm::vec3(color.r, color.g, color.b);
        material->tint.a = color.a;
    } else {
        material->albedo = glm::vec3(0.8f, 1.0f, 1.0f); // Default white
        material->tint.a = 1.0f;                        // Default opaque
    }

    if (aiMat->Get(AI_MATKEY_METALLIC_FACTOR, factor) == AI_SUCCESS) {
        material->metallic = factor;
    } else {
        material->metallic = 0.95f;
    }

    if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS) {
        material->roughness = factor;
    } else {
        material->roughness = 0.1f;
    }

    material->metallic = glm::clamp(material->metallic, 0.0f, 1.0f);
    material->roughness = glm::clamp(material->roughness, 0.04f, 1.0f);

    aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
        material->emission = aiToGlm(emissiveColor);
    }

    // Opacity and Transparency
    float opacity = 1.0f;
    if (aiMat->Get(AI_MATKEY_OPACITY, opacity) == AI_SUCCESS) {
        material->tint.a *= opacity; // Modulate tint alpha with overall opacity
    }
    material->transparent = (material->tint.a < 0.999f);

    float ambientOcclusion = 1.0f;
    if (aiMat->Get(AI_MATKEY_OPACITY, ambientOcclusion) == AI_SUCCESS) {
        material->ambientOcclusion = ambientOcclusion;
    } else {
        material->ambientOcclusion = 1.0f;
    }

    // --- Load Textures ---
    // Albedo / Base Color
    material->textureAlbedo = loadMaterialTexture(aiMat, scene, aiTextureType_BASE_COLOR).get();
    if (!material->textureAlbedo)
        material->textureAlbedo = loadMaterialTexture(aiMat, scene, aiTextureType_DIFFUSE).get();
    material->useTextureAlbedo = (material->textureAlbedo != nullptr);

    std::shared_ptr<Texture2D> tex;

    tex = loadMaterialTexture(aiMat, scene, aiTextureType_UNKNOWN, 0); // GLTF ORM texture
    if (!tex)
        tex = loadMaterialTexture(aiMat, scene, aiTextureType_SPECULAR,
                                  0); // Some formats pack roughness in specular alpha, metallic can be separate
    if (!tex)
        tex = loadMaterialTexture(aiMat, scene, aiTextureType_METALNESS,
                                  0); // Some formats use this for combined or just metallic

    if (tex) {
        // Heuristic: if it's UNKNOWN it's likely ORM. If METALNESS or SPECULAR, might be combined or just one
        // the PBR primary.
        material->textureMetallicRoughness = tex.get();
        material->useTextureMetallicRoughness = true;
    } else { // No obvious combined PBR map, try separate
        std::shared_ptr<Texture2D> texM = loadMaterialTexture(aiMat, scene, aiTextureType_METALNESS, 0);
        if (texM) {
            material->textureMetallic = texM.get();
            material->useTextureMetallic = true;
        }

        std::shared_ptr<Texture2D> texR = loadMaterialTexture(aiMat, scene, aiTextureType_DIFFUSE_ROUGHNESS, 0);
        if (!texR)
            texR = loadMaterialTexture(aiMat, scene, aiTextureType_SHININESS,
                                       0); // Shininess needs conversion to roughness
        if (texR) {
            material->textureRoughness = texR.get();
            material->useTextureRoughness = true;
        }
    }

    // Normals
    material->textureNormal = loadMaterialTexture(aiMat, scene, aiTextureType_NORMALS).get();
    if (!material->textureNormal)
        material->textureNormal = loadMaterialTexture(aiMat, scene, aiTextureType_HEIGHT).get();
    if (material->textureNormal)
        material->useTextureNormal = true;

    // Ambient Occlusion
    material->textureAmbientOcclusion = loadMaterialTexture(aiMat, scene, aiTextureType_AMBIENT_OCCLUSION).get();
    // Some GLTF files pack AO with MetallicRoughness, Assimp might expose it as
    // LIGHTMAP too for some reason
    if (!material->textureAmbientOcclusion)
        material->textureAmbientOcclusion = loadMaterialTexture(aiMat, scene, aiTextureType_LIGHTMAP).get();
    if (material->textureAmbientOcclusion)
        material->useTextureAmbientOcclusion = true;

    // Emissive
    material->textureEmissive = loadMaterialTexture(aiMat, scene, aiTextureType_EMISSIVE).get();
    if (material->textureEmissive)
        material->useTextureEmissive = true;

    // --- Pipeline State ---
    int twoSided = 0;
    if (aiMat->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS && twoSided) {
        material->pipelineState.faceCulling.enabled = false;
    } else {
        material->pipelineState.faceCulling.enabled = true; // Default: cull back faces
    }

    material->pipelineState.depthTesting.enabled = true;       // Default true
    material->pipelineState.depthTesting.function = GL_LEQUAL; // Default or
    // from material

    if (material->transparent) {
        material->pipelineState.blending.enabled = true;
        // Common alpha blending setup (can be configured further from material
        // if needed)
        material->pipelineState.blending.sourceFactor = GL_SRC_ALPHA;
        material->pipelineState.blending.destinationFactor = GL_ONE_MINUS_SRC_ALPHA;
        material->pipelineState.blending.equation = GL_FUNC_ADD;
        material->pipelineState.depthMask = GL_FALSE; // Typically no depth write for alpha blended
    } else {
        material->pipelineState.depthMask = GL_TRUE;
    }

    // Add lights
    auto lightsFromLoader = AssetLoader<Light>::getAll();
    for (const auto& [name, light_ptr] : lightsFromLoader) {
        material->lights.push_back(light_ptr);
    }

    return material;
}

// Helper function to convert Assimp texture map mode to OpenGL GLenum
GLenum assimpTextureMapModeToOpenGL(aiTextureMapMode mode) {
    switch (mode) {
    case aiTextureMapMode_Wrap:
        return GL_REPEAT;
    case aiTextureMapMode_Clamp:
        return GL_CLAMP_TO_EDGE;
    case aiTextureMapMode_Mirror:
        return GL_MIRRORED_REPEAT;
    case aiTextureMapMode_Decal:
        // For decal, you might choose GL_CLAMP_TO_EDGE or GL_REPEAT
        // or handle it specifically in your shader.
        // GL_CLAMP_TO_BORDER also exists if you set a border color.
        return GL_CLAMP_TO_EDGE;
    default:
        // Default to GL_REPEAT if mode is unknown or not specified
        return GL_REPEAT;
    }
}

std::shared_ptr<Texture2D> Model::loadMaterialTexture(const aiMaterial* aiMat, const aiScene* scene, aiTextureType type,
                                                      unsigned int index) {
    aiString pathFromAssimp;
    if (aiMat->GetTexture(type, index, &pathFromAssimp) == AI_SUCCESS) {
        std::string texturePathStr = aiToStr(pathFromAssimp);

        if (texturePathStr.rfind('*', 0) == 0) { // Starts with '*' indicates embedded texture
            unsigned int embeddedTextureIndex = 0;
            try {
                embeddedTextureIndex = std::stoi(texturePathStr.substr(1));
            } catch (const std::exception& e) {
                std::cerr << "[Model] ERROR: Invalid embedded texture index format: " << texturePathStr << " ("
                          << e.what() << ")" << std::endl;
                return nullptr;
            }

            if (scene && embeddedTextureIndex < scene->mNumTextures) {
                const aiTexture* embTex = scene->mTextures[embeddedTextureIndex];
                std::cout << "[Model] Found embedded texture: " << texturePathStr
                          << " (Format hint: " << embTex->achFormatHint << ", Dimensions: " << embTex->mWidth << "x"
                          << embTex->mHeight << ")" << std::endl;

                // Check cache first for embedded texture (using "*index" as
                // key)
                auto it = texture_cache.find(texturePathStr);
                if (it != texture_cache.end()) {
                    return it->second;
                }

                std::shared_ptr<Texture2D> embedded_texture;

                if (embTex->mHeight == 0) {
                    // Compressed format - use loadFromMemory
                    embedded_texture = std::shared_ptr<Texture2D>(
                        texture_utils::loadFromMemory(reinterpret_cast<const unsigned char*>(embTex->pcData),
                                                      embTex->mWidth, // Buffer size in bytes
                                                      true            // Generate mipmaps
                                                      ));
                } else {
                    // Uncompressed format - direct pixel data
                    // This is less common but can happen with some formats
                    std::cerr << "[Model] WARNING: Uncompressed embedded texture format not fully supported yet for "
                              << texturePathStr << std::endl;
                    // You could implement this case if needed by directly using glTexImage2D
                    // with the pixel data from embTex->pcData
                    return nullptr;
                }

                if (embedded_texture) {
                    texture_cache[texturePathStr] = embedded_texture; // Cache it
                    std::cout << "[Model] Successfully loaded embedded texture: " << texturePathStr << std::endl;
                    return embedded_texture;
                } else {
                    std::cerr << "[Model] ERROR: Failed to load embedded texture " << texturePathStr << std::endl;
                    return nullptr;
                }

            } else {
                std::cerr << "[Model] ERROR: Invalid embedded texture index " << embeddedTextureIndex
                          << " or scene is null." << std::endl;
                return nullptr;
            }
        } else {
            // Regular file path (relative to model file)
            std::shared_ptr<Texture2D> texture = loadTexture(texturePathStr);

            GLenum wrapS = GL_REPEAT;
            GLenum wrapT = GL_REPEAT;

            if (texture) {
                aiTextureMapMode aiWrapS;
                if (aiMat->Get(AI_MATKEY_MAPPINGMODE_U(type, index), aiWrapS) == AI_SUCCESS) {
                    wrapS = assimpTextureMapModeToOpenGL(aiWrapS);
                }

                aiTextureMapMode aiWrapT;
                if (aiMat->Get(AI_MATKEY_MAPPINGMODE_V(type, index), aiWrapT) == AI_SUCCESS) {
                    wrapT = assimpTextureMapModeToOpenGL(aiWrapT);
                }

                texture->bind();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
                texture->unbind();

            } else {
                std::cerr << "[Model] WARNING: Failed to load texture: " << texturePathStr << std::endl;
            }
            return texture;
        }
    }
    return nullptr; // No texture of this type found
}

std::shared_ptr<Texture2D> Model::loadTexture(const std::string& texturePathInModel) {
    // `texturePathInModel` is the path as it appears in the model file (e.g.,
    // relative, or *index for embedded)
    auto it = texture_cache.find(texturePathInModel);
    if (it != texture_cache.end()) {
        return it->second; // Already cached
    }

    std::string fullPath = directory + texturePathInModel;
    std::cout << "[Model] Attempting to load texture: " << texturePathInModel << " (Full path: " << fullPath << ")"
              << std::endl;

    std::shared_ptr<Texture2D> texture(texture_utils::loadImage(fullPath, true));

    if (!texture) {
        std::cerr << "[Model] WARNING: Texture not loaded: " << fullPath << ". It might not exist or" << std::endl;
        return nullptr;
    }

    std::cout << "[Model] Successfully loaded texture: " << fullPath << std::endl;
    texture_cache[texturePathInModel] = texture; // Cache it using the model-relative path as key
    return texture;
}

void Model::loadSkeletonFromScene(const aiScene* scene) {
    std::cout << "[Model] Loading skeleton from scene..." << std::endl;

    std::map<std::string, glm::mat4> boneOffsetMatrices;
    std::set<std::string> boneNamesFromMeshes;

    // Stage 1: Collect all bones from meshes and their offset matrices
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        const aiMesh* mesh = scene->mMeshes[i];
        for (unsigned int j = 0; j < mesh->mNumBones; ++j) {
            const aiBone* assimpBone = mesh->mBones[j];
            std::string boneName = assimpBone->mName.C_Str();

            boneNamesFromMeshes.insert(boneName);
            if (boneOffsetMatrices.find(boneName) == boneOffsetMatrices.end()) {
                boneOffsetMatrices[boneName] = aiToGlm(assimpBone->mOffsetMatrix);
            }
        }
    }

    if (boneNamesFromMeshes.empty()) {
        std::cout << "[Model] No bones found in meshes for skeleton." << std::endl;
        return;
    }
    std::cout << "[Model] Found " << boneNamesFromMeshes.size() << " unique bones in meshes." << std::endl;

    // Stage 2: Recursively build the skeleton hierarchy from the aiNode tree
    // The skeleton member 'this->skeleton' will be populated by processNodeForSkeleton
    processNodeForSkeleton(scene->mRootNode, -1, boneOffsetMatrices, boneNamesFromMeshes);

    // Optional: Initialize bind pose transforms if your Skeleton class requires it
    // this->skeleton.calculateBoneTransforms(); // If you want to pre-calculate bind pose

    // Verification: Print skeleton hierarchy
    std::cout << "[Model] Skeleton loaded. Hierarchy:" << std::endl;
    this->skeleton.printHierarchy();
    this->skeleton.validateHierarchy();
    this->skeleton.logSkeletonInfo();
}

void Model::processNodeForSkeleton(const aiNode* assimpNode, int parentBoneIndexInSkeleton,
                                   const std::map<std::string, glm::mat4>& boneOffsetMatrices,
                                   const std::set<std::string>& boneNamesFromMeshes) {
    std::string nodeName = assimpNode->mName.C_Str();
    int currentBoneIndexInSkeleton = parentBoneIndexInSkeleton; // Pass down parent's ID by default

    // Check if this node corresponds to a bone that affects meshes
    if (boneNamesFromMeshes.count(nodeName)) {
        Bone newBone; // Use your Bone struct
        newBone.name = nodeName;
        newBone.localBindTransform = aiToGlm(assimpNode->mTransformation);

        // Check if offset matrix exists for this bone (it should if it's in boneNamesFromMeshes)
        auto it = boneOffsetMatrices.find(nodeName);
        if (it != boneOffsetMatrices.end()) {
            newBone.offsetMatrix = it->second;
        } else {
            // This case should ideally not happen if boneNamesFromMeshes was populated correctly
            // Or this node is part of the hierarchy but doesn't directly have weights (e.g. an empty).
            // For now, let's give it an identity offset matrix if not found, though this might be an error.
            std::cerr << "[Model] WARNING: Offset matrix not found for bone: " << nodeName << ". Using identity."
                      << std::endl;
            newBone.offsetMatrix = glm::mat4(1.0f);
        }

        newBone.parentIndex = parentBoneIndexInSkeleton;
        // The ID and children will be set by skeleton.addBone()

        currentBoneIndexInSkeleton = this->skeleton.addBone(newBone);
    } else {
        // This node is not a bone itself but part of the hierarchy.
        // Its transform will be accumulated by its children that are bones.
        // No change to currentBoneIndexInSkeleton, so children of this node
        // will be parented to parentBoneIndexInSkeleton (or its bone ancestor).
        // This behavior is correct if we only want bones that deform meshes in the 'bones' list.
        // If we want all nodes that are part of an armature to be "bones", even if they don't deform,
        // the logic would need to check node type or rely on aiProcess_PopulateArmatureData differently.
        // For now, we stick to plan: bones are those in aiMesh->mBones.
    }

    // Recursively process children
    for (unsigned int i = 0; i < assimpNode->mNumChildren; ++i) {
        processNodeForSkeleton(assimpNode->mChildren[i], currentBoneIndexInSkeleton, boneOffsetMatrices,
                               boneNamesFromMeshes);
    }
}

void Model::processVertexBoneData(const aiMesh* mesh, std::vector<Vertex>& vertices) {
    if (!mesh->HasBones()) {
        return;
    }

    std::cout << "[Model] [Bone Mapping] Processing vertex bone data for mesh: '" << aiToStr(mesh->mName)
              << "' (contains " << mesh->mNumBones << " bones in its local list)." << std::endl;

    for (unsigned int meshLocalBoneIdx = 0; meshLocalBoneIdx < mesh->mNumBones; ++meshLocalBoneIdx) {
        const aiBone* assimpBone = mesh->mBones[meshLocalBoneIdx];
        std::string boneName = aiToStr(assimpBone->mName);

        int skeletonGlobalBoneIdx = this->skeleton.findBoneIndex(boneName);

        if (skeletonGlobalBoneIdx == -1) {
            // This is an important check: a bone defined in the mesh's bone list
            // is not found in the globally constructed skeleton. This could happen if
            // the bone wasn't part of the main hierarchy traversal (e.g., an unattached bone)
            // or if there's a naming mismatch not caught earlier.
            std::cout << "[Model] [Bone Mapping]  WARNING: Mesh bone '" << boneName
                      << "' (local mesh bone index: " << meshLocalBoneIdx << ") from mesh '" << aiToStr(mesh->mName)
                      << "' was NOT FOUND in the global skeleton. Vertex weights for this bone will be SKIPPED."
                      << std::endl;
            continue;
        }

        // Verification Print: "Print bone mapping for a simple rigged model"
        // This shows the mapping from a bone's name (and its local index in the aiMesh)
        // to its global index in your `Model::skeleton`.
        // std::cout << "[Model] [Bone Mapping]  Mesh '" << aiToStr(mesh->mName) << "': Bone '" << boneName
        //           << "' (local mesh idx " << meshLocalBoneIdx
        //           << ") ==> Global Skeleton Index: " << skeletonGlobalBoneIdx << std::endl;

        for (unsigned int weightIdx = 0; weightIdx < assimpBone->mNumWeights; ++weightIdx) {
            const aiVertexWeight& assimpWeight = assimpBone->mWeights[weightIdx];
            unsigned int vertexId = assimpWeight.mVertexId;

            if (vertexId >= vertices.size()) {
                std::cerr << "[Model] [Bone Mapping]  ERROR: Invalid vertexId " << vertexId
                          << " from bone weights for bone '" << boneName << "' in mesh '" << aiToStr(mesh->mName)
                          << "'." << std::endl;
                continue;
            }

            Vertex& targetVertex = vertices[vertexId];
            bool influenceAdded = false;
            for (int influenceSlot = 0; influenceSlot < MAX_BONE_INFLUENCE; ++influenceSlot) {
                if (targetVertex.bone_ids[influenceSlot] == -1) { // Check if slot is empty (assuming -1 means empty)
                                                                  // Or use targetVertex.weights[influenceSlot] == 0.0f
                    // if -1 is a valid bone ID (e.g. for non-influenced root)
                    targetVertex.bone_ids[influenceSlot] = skeletonGlobalBoneIdx;
                    targetVertex.weights[influenceSlot] = assimpWeight.mWeight;
                    influenceAdded = true;
                    break;
                }
            }
            if (!influenceAdded) {
            }
        }
    }

    // Verification Print: "Verify that vertex bone indices reference valid skeleton bones"
    // This can be done by checking a sample of vertices after all bones for the mesh are processed.
    if (!vertices.empty() && mesh->HasBones()) {
        std::cout << "[Model] [Bone Mapping]  Vertex Bone ID/Weight Verification for first few vertices of mesh '"
                  << aiToStr(mesh->mName) << "':" << std::endl;
        int numVerticesToInspect = std::min((int)vertices.size(), 3); // Inspect up to 3 vertices
        for (int vIdx = 0; vIdx < numVerticesToInspect; ++vIdx) {
            const auto& vert = vertices[vIdx];
            std::cout << "[Model] [Bone Mapping]    Vertex " << vIdx << ": IDs=[";
            float totalWeight = 0.0f;
            for (int j = 0; j < MAX_BONE_INFLUENCE; ++j) {
                std::cout << vert.bone_ids[j];
                if (vert.bone_ids[j] != -1) { // If the bone ID is not "unused"
                    if (vert.bone_ids[j] < 0 || vert.bone_ids[j] >= (int)this->skeleton.getBoneCount()) {
                        std::cout << "(INVALID_ID!)"; // Critical error if an ID is out of bounds
                    }
                    totalWeight += vert.weights[j];
                }
                if (j < MAX_BONE_INFLUENCE - 1)
                    std::cout << ", ";
            }
            std::cout << "], Weights=[";
            for (int j = 0; j < MAX_BONE_INFLUENCE; ++j) {
                std::cout << std::fixed << std::setprecision(2) << vert.weights[j] << std::fixed
                          << std::setprecision(6); // Show weights with precision
                if (j < MAX_BONE_INFLUENCE - 1)
                    std::cout << ", ";
            }
            std::cout << "], TotalWeight=" << std::fixed << std::setprecision(3) << totalWeight << std::fixed
                      << std::setprecision(6) << std::endl;
            if (totalWeight > 0.0f && (totalWeight < 0.99f || totalWeight > 1.01f)) {
                std::cout << "[Model] [Bone Mapping]      WARNING: Vertex " << vIdx << " weights sum to " << totalWeight
                          << ", not ~1.0." << std::endl;
            }
        }
    }
    std::cout << "[Model] [Bone Mapping] Finished processing vertex bone data for mesh: '" << aiToStr(mesh->mName)
              << "'" << std::endl;
}

void Model::loadAnimationsFromScene(const aiScene* scene) {
    if (!scene->HasAnimations()) {
        std::cout << "[Model] Scene has no animations." << std::endl;
        return;
    }

    std::cout << "[Model] Loading " << scene->mNumAnimations << " animation(s)..." << std::endl;
    this->animations.reserve(scene->mNumAnimations);

    for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
        const aiAnimation* assimpAnimation = scene->mAnimations[i];
        Animation currentLoadedAnimation;

        currentLoadedAnimation.name = aiToStr(assimpAnimation->mName);
        currentLoadedAnimation.duration = static_cast<float>(assimpAnimation->mDuration);
        currentLoadedAnimation.ticksPerSecond = static_cast<float>(assimpAnimation->mTicksPerSecond);
        if (currentLoadedAnimation.ticksPerSecond == 0) {
            currentLoadedAnimation.ticksPerSecond = 25.0f; // Default to 25 FPS if not specified
            std::cout << "[Model] Animation '" << currentLoadedAnimation.name
                      << "' has 0 ticksPerSecond, defaulting to " << currentLoadedAnimation.ticksPerSecond << std::endl;
        }

        std::cout << "[Model]  Loading Animation: '" << currentLoadedAnimation.name
                  << "', Duration: " << currentLoadedAnimation.duration << " ticks"
                  << ", Ticks/Sec: " << currentLoadedAnimation.ticksPerSecond
                  << ", Channels: " << assimpAnimation->mNumChannels << std::endl;

        currentLoadedAnimation.boneAnimations.reserve(assimpAnimation->mNumChannels);

        for (unsigned int j = 0; j < assimpAnimation->mNumChannels; ++j) {
            const aiNodeAnim* nodeAnim = assimpAnimation->mChannels[j];
            BoneAnimation boneAnimTrack;
            boneAnimTrack.boneName = aiToStr(nodeAnim->mNodeName);

            if (this->skeleton.findBoneIndex(boneAnimTrack.boneName) == -1) {
                std::cout << "[Model]   Warning: Animation channel for node '" << boneAnimTrack.boneName
                          << "' which is not in the loaded skeleton. Still loading channel." << std::endl;
            }

            // Collect all unique timestamps from position, rotation, and scale keys
            std::set<float> uniqueTimestamps;
            for (unsigned int k = 0; k < nodeAnim->mNumPositionKeys; ++k)
                uniqueTimestamps.insert(static_cast<float>(nodeAnim->mPositionKeys[k].mTime));
            for (unsigned int k = 0; k < nodeAnim->mNumRotationKeys; ++k)
                uniqueTimestamps.insert(static_cast<float>(nodeAnim->mRotationKeys[k].mTime));
            for (unsigned int k = 0; k < nodeAnim->mNumScalingKeys; ++k)
                uniqueTimestamps.insert(static_cast<float>(nodeAnim->mScalingKeys[k].mTime));

            if (uniqueTimestamps.empty() &&
                (nodeAnim->mNumPositionKeys > 0 || nodeAnim->mNumRotationKeys > 0 || nodeAnim->mNumScalingKeys > 0)) {
                // This case implies keys exist but their times weren't inserted, unlikely with float conversion.
                // Or all keys are at time 0.
                // If all keys are at time 0 and uniqueTimestamps is empty (e.g. if keys were non-float before casting),
                // we might need to manually add 0.0f if any keys exist.
                // However, `std::set<float>` from `double` times should be fine.
            }
            if (uniqueTimestamps.empty() && nodeAnim->mNumPositionKeys == 0 && nodeAnim->mNumRotationKeys == 0 &&
                nodeAnim->mNumScalingKeys == 0) {
                // This channel has no keyframes for this bone.
                // std::cout << "[Model]   Channel '" << boneAnimTrack.boneName << "' has no keyframes." << std::endl;
                // continue; // Skip if no keyframes for this bone at all
            }

            // If uniqueTimestamps is empty but there's at least one key (e.g., a single static pose key at time 0),
            // add time 0.0f to ensure at least one KeyFrame is generated.
            // This handles static poses defined by a single keyframe at t=0.
            if (uniqueTimestamps.empty() &&
                (nodeAnim->mNumPositionKeys > 0 || nodeAnim->mNumRotationKeys > 0 || nodeAnim->mNumScalingKeys > 0)) {
                uniqueTimestamps.insert(0.0f);
            }

            boneAnimTrack.keyFrames.reserve(uniqueTimestamps.size());

            // For each unique timestamp, create a KeyFrame by interpolating P, R, S
            for (float t : uniqueTimestamps) {
                KeyFrame keyFrame;
                keyFrame.timeStamp = t;
                keyFrame.position = interpolatePosition(t, nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys);
                keyFrame.rotation = interpolateRotation(t, nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys);
                keyFrame.scale = interpolateScale(t, nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys);
                boneAnimTrack.keyFrames.push_back(keyFrame);
            }

            currentLoadedAnimation.boneAnimations.push_back(boneAnimTrack);
            std::cout << "[Model]    Channel for bone: '" << boneAnimTrack.boneName << "' loaded with "
                      << boneAnimTrack.keyFrames.size() << " combined keyframes." << std::endl;
        }
        this->animations.push_back(currentLoadedAnimation);
    }
    std::cout << "[Model] Finished loading animations." << std::endl;

    // Verification Step
    std::cout << "[Model] === Animation Data Verification ===" << std::endl;
    for (const auto& anim : this->animations) {
        std::cout << "Animation Name: " << anim.name << ", Duration (ticks): " << anim.duration
                  << ", Ticks/Sec: " << anim.ticksPerSecond << ", Bone Animation Tracks: " << anim.boneAnimations.size()
                  << std::endl;
        for (const auto& boneAnim : anim.boneAnimations) {
            std::cout << "  Bone: " << boneAnim.boneName << ", Keyframes: " << boneAnim.keyFrames.size() << std::endl;
        }
    }
    std::cout << "[Model] ===================================" << std::endl;
}

glm::mat4 Model::aiToGlm(const aiMatrix4x4& m) {
    return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}

} // namespace our
