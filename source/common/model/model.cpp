#include "model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>
#include <asset-loader.hpp>
#include <ecs/entity.hpp>
#include <settings.hpp>
#include "application.hpp"
#include "glad/gl.h"
#include "glm/common.hpp"
#include "material/material.hpp"
#include "texture/texture-utils.hpp"
#include "texture/texture2d.hpp"

namespace our {

Model::~Model() {
    for (auto* mr : meshRenderers)
        delete mr;
}

bool Model::loadFromFile(const std::string& path) {
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

    loadMaterialsFromScene(scene);

    processNode(scene->mRootNode, scene, glm::mat4(1.0f));

    generateCombinedMesh();
    std::cout << "[Model] Generated combined mesh with " << (combinedMesh ? combinedMesh->cpuVertices.size() : 0)
              << " vertices." << std::endl;
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
            v.color = glm::vec4(1.0f);
        // v.color =
        // glm::vec4(mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a);

        verts.push_back(v);
    }

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

    // Calculate View and Projection matrices once
    glm::mat4 projectionMatrix = camera->getProjectionMatrix(windowSize);
    glm::mat4 viewMatrix = camera->getViewMatrix();
    glm::mat4 viewProjectionMatrix = projectionMatrix * viewMatrix; // VP

    // Get camera world position
    // Assuming getLocalToWorldMatrix() returns the world transformation of the
    // camera's owner. The 4th column (index 3) of a 4x4 transformation matrix
    // is the translation vector.
    glm::vec3 cameraWorldPosition = glm::vec3(camera->getOwner()->getLocalToWorldMatrix()[3]);

    for (const auto& meshRendererUniquePtr : meshRenderers) {
        // Get the raw pointer from unique_ptr for convenience
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

        // Setup material (sets pipeline state and uses shader)
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
        meshRenderer->material->shader->set("model",
                                            modelMatrix);        // Model Matrix
        meshRenderer->material->shader->set("view", viewMatrix); // View Matrix
        meshRenderer->material->shader->set("projection",
                                            projectionMatrix); // Projection Matrix
        meshRenderer->material->shader->set("MV",
                                            modelViewMatrix);            // Model-View Matrix
        meshRenderer->material->shader->set("VP", viewProjectionMatrix); // View-Projection Matrix
        meshRenderer->material->shader->set("transform",
                                            modelViewProjectionMatrix); // Commonly used name for MVP

        // Other common uniforms
        meshRenderer->material->shader->set("cameraPosition", cameraWorldPosition);

        // Application-specific uniforms (like bloom)
        // Check if the uniform exists before setting to avoid OpenGL errors if
        // shader doesn't declare it
        meshRenderer->material->shader->set("bloomBrightnessCutoff", bloomCutoff);

        Settings& settings = Settings::getInstance();
        meshRenderer->material->shader->set("debugMode", settings.shaderDebugModeToInt(settings.shaderDebugMode));

        // Note: If LitMaterial (or other materials) sets its own specific
        // matrices or camera position in its `setup()` method, there might be
        // redundancy or overrides. Typically, these common matrices are set in
        // the Model's draw loop, and material->setup() handles
        // material-specific properties (textures, colors, lighting params).

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
    // For now, we'll default to LitMaterial as it's the most comprehensive.
    // You could add logic here to choose different material types based on
    // aiMaterial properties.
    auto material = std::make_unique<LitMaterial>();

    material->transparent = false; // Default to opaque
    material->useTextureAlbedo = false;
    material->useTextureMetallicRoughness = false;
    material->useTextureNormal = false;
    material->useTextureAmbientOcclusion = false;
    material->useTextureEmissive = false;
    material->useTextureMetallic = false;
    material->useTextureRoughness = false;
    material->metallic = 0.95f;
    material->roughness = 0.1f;

    aiString matName;
    if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS) {
        std::cout << "[Model] Processing material: " << aiToStr(matName) << std::endl;
    }

    // Assign a default shader (e.g., PBR shader)
    // This could be made more configurable, e.g., from material properties if
    // the format supports it.
    material->shader = AssetLoader<ShaderProgram>::get("pbr");
    if (!material->shader) {
        std::cerr << "[Model] WARNING: Default PBR shader not found for material: " << aiToStr(matName) << std::endl;
        // Fallback or error
        material->shader = AssetLoader<ShaderProgram>::get("default"); // Try a simpler default
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
        material->metallic = 0.95f; // Default metallic
    }

    if (aiMat->Get(AI_MATKEY_ROUGHNESS_FACTOR, factor) == AI_SUCCESS) {
        material->roughness = factor;
    } else {
        material->roughness = 0.1f; // Default roughness
    }

    material->metallic = glm::clamp(material->metallic, 0.0f, 1.0f);
    material->roughness = glm::clamp(material->roughness, 0.04f, 1.0f);

    aiColor3D emissiveColor(0.0f, 0.0f, 0.0f);
    if (aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveColor) == AI_SUCCESS) {
        material->emission = aiToGlm(emissiveColor);
    }

    // occlusion factor

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
        material->ambientOcclusion = 1.0f; // Default ambient occlusion
    }

    // --- Load Textures ---
    // Albedo / Base Color
    material->textureAlbedo = loadMaterialTexture(aiMat, scene, aiTextureType_BASE_COLOR).get();
    if (!material->textureAlbedo)
        material->textureAlbedo = loadMaterialTexture(aiMat, scene, aiTextureType_DIFFUSE).get(); // Fallback
    material->useTextureAlbedo = (material->textureAlbedo != nullptr);

    // Metallic-Roughness or Separate Metallic and Roughness
    // aiString pathMetallicStr, pathRoughnessStr;
    // bool hasMetallicPath = (aiMat->GetTexture(aiTextureType_METALNESS, 0, &pathMetallicStr) == AI_SUCCESS);
    //
    // bool hasRoughnessPath = (aiMat->GetTexture(aiTextureType_DIFFUSE_ROUGHNESS, 0, &pathRoughnessStr) == AI_SUCCESS);
    //
    // bool hasCombinedPath =
    //     (aiMat->GetTexture(aiTextureType_UNKNOWN, 0, &pathMetallicStr) == AI_SUCCESS); // Check for combined texture
    // //
    //
    // if (hasMetallicPath && hasRoughnessPath &&
    //     std::string(pathMetallicStr.C_Str()) == std::string(pathRoughnessStr.C_Str())) {
    //
    //     // Same path, likely a combined MetallicRoughness texture (common in
    //     // glTF)
    //     std::cout << "[Model] Using combined metallic-roughness texture." << std::endl;
    //     material->textureMetallicRoughness = loadMaterialTexture(aiMat, scene, aiTextureType_UNKNOWN).get();
    //     material->useTextureMetallicRoughness = true;
    // } else {
    //     std::cout << "[Model] Using separate metallic and roughness textures. hasMetallicPath: " << hasMetallicPath
    //               << ", hasRoughnessPath: " << hasRoughnessPath << "hasCombinedPath: " << hasCombinedPath <<
    //               std::endl;
    //     if (hasMetallicPath) {
    //         material->textureMetallic = loadMaterialTexture(aiMat, scene, aiTextureType_METALNESS).get();
    //         if (material->textureMetallic)
    //             material->useTextureMetallic = true;
    //     }
    //     if (hasRoughnessPath) {
    //         material->textureRoughness = loadMaterialTexture(aiMat, scene, aiTextureType_DIFFUSE_ROUGHNESS).get();
    //         if (material->textureRoughness)
    //             material->useTextureRoughness = true;
    //     }
    //     if (!hasMetallicPath && !hasRoughnessPath) {
    //         material->textureMetallicRoughness = loadMaterialTexture(aiMat, scene, aiTextureType_UNKNOWN).get();
    //         if (material->textureMetallicRoughness)
    //             material->useTextureMetallicRoughness = true;
    //     }
    // }
    //
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
        // component. This part is tricky and format-dependent. For simplicity, assume if one of these is found, it's
        // the PBR primary.
        material->textureMetallicRoughness = tex.get();
        material->useTextureMetallicRoughness = true;
    } else { // No obvious combined PBR map, try separate
        std::cout << "[Model] Trying Using separate metallic and roughness textures." << std::endl;
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
        material->textureNormal =
            loadMaterialTexture(aiMat, scene, aiTextureType_HEIGHT).get(); // Some formats use height for bump
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

    material->pipelineState.depthTesting.enabled = true; // Default true
    // material->pipelineState.depthTesting.function = GL_LEQUAL; // Default or
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

    // Add lights (typically global, from AssetLoader)
    // This part is retained from your LitMaterial's previous deserialize logic
    auto lightsFromLoader = AssetLoader<Light>::getAll();
    for (const auto& [name, light_ptr] : lightsFromLoader) {
        material->lights.push_back(light_ptr);
    }

    std::cout << "Using texture metallic " << material->useTextureMetallic << std::endl;
    std::cout << "Using texture roughness " << material->useTextureRoughness << std::endl;
    std::cout << "Using texture metallic roughness " << material->useTextureMetallicRoughness << std::endl;
    std::cout << "==========================" << std::endl;

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

                // Loading embedded textures:
                // Assimp stores compressed formats (jpg, png) with mHeight = 0.
                // mWidth is the buffer size. Uncompressed (raw pixels like
                // ARGB8888) have mHeight > 0. This requires AssetLoader to have
                // a method like loadFromMemory. For example:
                // std::shared_ptr<Texture2D> embedded_texture =
                // AssetLoader<Texture2D>::loadFromMemory(
                //     reinterpret_cast<const unsigned char*>(embTex->pcData),
                //     embTex->mWidth, // This is the size in bytes of pcData if
                //     mHeight is 0 embTex->achFormatHint // Hint for stb_image
                //     or similar
                // );
                // if (embedded_texture) {
                //    mTextureCache[texturePathStr] = embedded_texture; // Cache
                //    it return embedded_texture;
                // } else {
                //    std::cerr << "[Model] WARNING: Failed to load embedded
                //    texture "
                //    << texturePathStr << ". AssetLoader may not support it."
                //    << std::endl; return nullptr;
                // }
                std::cerr << "[Model] INFO: Embedded texture loading for '" << texturePathStr
                          << "' is sketched but likely requires "
                             "AssetLoader<Texture2D>::loadFromMemory to be "
                             "implemented."
                          << std::endl;
                return nullptr; // Placeholder until embedded loading is fully
                                // supported via AssetLoader

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

    // Try loading using AssetLoader. It should be configured to load from file
    // paths. The AssetLoader should ideally handle caching internally if `get`
    // is called multiple times with the same path. If AssetLoader caches by
    // name, then our mTextureCache is still useful if AssetLoader's name isn't
    // the full path. For simplicity, we assume
    // AssetLoader<Texture2D>::get(fullPath) works and might cache. Our
    // mTextureCache ensures we don't even *try* to load it again via
    // AssetLoader if already successful for this model.
    std::shared_ptr<Texture2D> texture(texture_utils::loadImage(fullPath, true));

    if (!texture) {
        std::cerr << "[Model] WARNING: Texture not loaded by AssetLoader: " << fullPath
                  << ". It might not exist or AssetLoader is not configured for "
                     "path loading."
                  << std::endl;
        // You could return a default "missing" texture here:
        // texture = AssetLoader<Texture2D>::get("default-missing-texture");
        return nullptr;
    }

    std::cout << "[Model] Successfully loaded texture: " << fullPath << std::endl;
    texture_cache[texturePathInModel] = texture; // Cache it using the model-relative path as key
    return texture;
}

glm::mat4 Model::aiToGlm(const aiMatrix4x4& m) {
    return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}

} // namespace our
