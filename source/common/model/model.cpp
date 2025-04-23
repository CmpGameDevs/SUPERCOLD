#include "model.hpp"

namespace our {

void Model::draw(CameraComponent* camera, glm::mat4 localToWorld, glm::ivec2 windowSize, float bloomBrightnessCutoff)
{
        // Go over all meshes and draw each one
        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 projection = camera->getProjectionMatrix(windowSize);
        glm::mat4 VP = projection * view;
        for (unsigned int i = 0; i < meshRenderers.size(); i++)
        {

            Material* material = meshRenderers[i]->material;
            Mesh* mesh = meshRenderers[i]->mesh;
            glm::mat4 meshWorldMatrix = localToWorld * matricesMeshes[i];
            glm::mat4 MVP = VP * meshWorldMatrix;
            material->setup();
            material->shader->set("transform", MVP);
            material->shader->set("cameraPosition", camera->getOwner()->localTransform.position);
            material->shader->set("view", view);
            material->shader->set("projection", projection);
            material->shader->set("model", meshWorldMatrix);
            material->shader->set("bloomBrightnessCutoff", bloomBrightnessCutoff);
            mesh->draw();
        }
}

// Reads a text file and outputs a string with everything in the text file
std::string Model::get_file_contents(std::string path) {
    std::ifstream file(path);
    if (!file) {
        std::cerr << "Could not open file: " << path << std::endl;
        return "";
    }
    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    return contents;
}

std::vector<unsigned char> Model::get_file_binary_contents(const std::string& path) {
    // Open file in binary mode with positioning at the end
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "ERROR: Could not open binary file: " << path << std::endl;
        return {};
    }

    // Get file size
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read the data
    std::vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        std::cerr << "ERROR: Failed to read binary file: " << path << std::endl;
        return {};
    }

    return buffer;
}

std::vector<unsigned char> Model::getData() {
    try {
        // Get the URI of the .bin file
        if (!JSON.contains("buffers") || JSON["buffers"].empty() || !JSON["buffers"][0].contains("uri")) {
            std::cerr << "ERROR: Missing buffer URI in GLTF" << std::endl;
            return {};
        }

        std::string uri = JSON["buffers"][0]["uri"].get<std::string>();
        std::string fileDirectory = path.substr(0, path.find_last_of('/') + 1);
        std::string fullPath = fileDirectory + uri;
        
        // Read binary data directly into a byte vector
        std::vector<unsigned char> binData = get_file_binary_contents(fullPath);
        
        return binData;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: Failed to load binary data: " << e.what() << std::endl;
        return {};
    }
}

void Model::loadTextures() {
    std::string directory = path.substr(0, path.find_last_of('/') + 1);

    const auto &images = JSON["images"];
    const auto &samplers = JSON["samplers"];
    const auto &texturesJSON = JSON["textures"];

    textures.reserve(texturesJSON.size());

    for (size_t i = 0; i < texturesJSON.size(); ++i) {
        const auto &textureInfo = texturesJSON[i];

        // Get image index
        int sourceIndex = textureInfo.contains("source") ? textureInfo["source"].get<int>() : -1;
        if (sourceIndex < 0 || sourceIndex >= images.size()) {
            std::cerr << "Invalid source index for texture " << i << "\n";
            textures.push_back(nullptr);
            continue;
        }

        // Get image path
        std::string uri = images[sourceIndex]["uri"];
        std::string imagePath = directory + uri;

        // Load image
        Texture2D *texture = texture_utils::loadImage(imagePath, true);
        if (!texture) {
            std::cerr << "Failed to load texture image: " << imagePath << "\n";
            textures.push_back(nullptr);
            continue;
        }

        // Get sampler index and apply settings if present
        if (textureInfo.contains("sampler")) {
            int samplerIndex = textureInfo["sampler"].get<int>();
            if (samplerIndex >= 0 && samplerIndex < samplers.size()) {
                const auto &sampler = samplers[samplerIndex];

                GLint minFilter = sampler.contains("minFilter") ? sampler["minFilter"].get<int>() : GL_LINEAR_MIPMAP_LINEAR;
                GLint magFilter = sampler.contains("magFilter") ? sampler["magFilter"].get<int>() : GL_LINEAR;
                GLint wrapS = sampler.contains("wrapS") ? sampler["wrapS"].get<int>() : GL_REPEAT;
                GLint wrapT = sampler.contains("wrapT") ? sampler["wrapT"].get<int>() : GL_REPEAT;

                texture->bind();
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
                texture->unbind();
            }
        }

        textures.push_back(texture);
    }
}

void Model::loadMesh(unsigned int indMesh) {
    // Get all accessor indices
    json primitive = JSON["meshes"][indMesh]["primitives"][0];
    json attributes = primitive["attributes"];
    
    // Check if the truly required attributes exist (position and indices)
    if (!attributes.contains("POSITION") || !primitive.contains("indices")) {
        std::cerr << "Mesh #" << indMesh << " missing position or indices" << std::endl;
        return;  // Skip this mesh - can't render without vertices or indices
    }

    if (!primitive.contains("material")) {
        std::cerr << "Mesh #" << indMesh << " missing material" << std::endl;
        return;  // Skip this mesh
    }
    
    unsigned int posAccInd = attributes["POSITION"];
    unsigned int indAccInd = primitive["indices"];
    unsigned int matInd = primitive["material"];

    // Load position data (always required)
    std::vector<float> posVec = getFloats(JSON["accessors"][posAccInd]);
    std::vector<glm::vec3> positions = groupFloatsVec3(posVec);
    
    // Load or create normals
    std::vector<glm::vec3> normals;
    if (attributes.contains("NORMAL")) {
        unsigned int normalAccInd = attributes["NORMAL"];
        std::vector<float> normalVec = getFloats(JSON["accessors"][normalAccInd]);
        normals = groupFloatsVec3(normalVec);
    } else {
        // Create default normals (all pointing up)
        normals.resize(positions.size(), glm::vec3(0.0f, 1.0f, 0.0f));
        std::cout << "Mesh #" << indMesh << " using default normals" << std::endl;
    }
    
    // Load or create texture coordinates
    std::vector<glm::vec2> texUVs;
    if (attributes.contains("TEXCOORD_0")) {
        unsigned int texAccInd = attributes["TEXCOORD_0"];
        std::vector<float> texVec = getFloats(JSON["accessors"][texAccInd]);
        texUVs = groupFloatsVec2(texVec);
    } else {
        // Create default UV coordinates (all 0,0)
        texUVs.resize(positions.size(), glm::vec2(0.0f, 0.0f));
        std::cout << "Mesh #" << indMesh << " using default texture coordinates (0,0)" << std::endl;
    }

    // Combine all the vertex components and also get the indices and textures
    std::vector<Vertex> vertices = assembleVertices(positions, normals, texUVs);
    std::vector<GLuint> indices = getIndices(JSON["accessors"][indAccInd]);

    // Combine the vertices, indices, and textures into a mesh
    MeshRendererComponent *meshRenderer = new MeshRendererComponent();
    meshRenderer->mesh = new Mesh(vertices, indices);
    meshRenderer->material = materials[matInd];
    meshRenderers.push_back(meshRenderer);
}

void Model::loadMaterials() {
    // Go over all materials
    for (int i = 0; i < JSON["materials"].size(); i++) {
        // Get the material properties
        json material = JSON["materials"][i];
        LitMaterial *mat = new LitMaterial();

        mat->transparent = false; // Default to opaque
        mat->useTextureAlbedo = false;
        mat->useTextureMetallicRoughness = false;
        mat->useTextureNormal = false;
        mat->useTextureAmbientOcclusion = false;
        mat->useTextureEmissive = false;
        mat->useTextureMetallic = false;
        mat->useTextureRoughness = false;


        // Set alpha mode if available
        if (material.find("alphaMode") != material.end()) {
            std::string alphaMode = material["alphaMode"].get<std::string>();
            mat->transparent = (alphaMode == "BLEND" || alphaMode == "MASK");
        }

        // Handle PBR Metallic Roughness
        if (material.find("pbrMetallicRoughness") != material.end()) {
            json pbr = material["pbrMetallicRoughness"];
            
            // Base color texture
            if (pbr.find("baseColorTexture") != pbr.end()) {
                unsigned int texInd = pbr["baseColorTexture"]["index"];
                mat->textureAlbedo = textures[texInd];
                mat->useTextureAlbedo = true;
            }
            
            // Metallic roughness texture
            if (pbr.find("metallicRoughnessTexture") != pbr.end()) {
                unsigned int texInd = pbr["metallicRoughnessTexture"]["index"];
                mat->textureMetallicRoughness = textures[texInd];
                mat->useTextureMetallicRoughness = true;
            }

            // Base color factor
            if (pbr.find("baseColorFactor") != pbr.end()) {
                glm::vec4 baseColor;
                for (unsigned int j = 0; j < pbr["baseColorFactor"].size(); j++) {
                    baseColor[j] = pbr["baseColorFactor"][j];
                }
                mat->albedo = glm::vec3(baseColor);
            }

            // Metallic factor
            if (pbr.find("metallicFactor") != pbr.end()) {
                mat->metallic = pbr["metallicFactor"];
            }

            // Roughness factor
            if (pbr.find("roughnessFactor") != pbr.end()) {
                mat->roughness = pbr["roughnessFactor"];
            }
        }

        // Normal texture
        if (material.find("normalTexture") != material.end()) {
            unsigned int texInd = material["normalTexture"]["index"];
            mat->textureNormal = textures[texInd];
            mat->useTextureNormal = true;
        }

        // Occlusion texture
        if (material.find("occlusionTexture") != material.end()) {
            unsigned int texInd = material["occlusionTexture"]["index"];
            mat->textureAmbientOcclusion = textures[texInd];
            mat->useTextureAmbientOcclusion = true;
        }

        // Occlusion strength
        if (material.find("occlusionFactor") != material.end()) {
            mat->ambientOcclusion = material["occlusionFactor"];
        }

        // Emissive texture and factor
        if (material.find("emissiveTexture") != material.end()) {
            unsigned int texInd = material["emissiveTexture"]["index"];
            mat->textureEmissive = textures[texInd];
            mat->useTextureEmissive = true;
        }
        
        if (material.find("emissiveFactor") != material.end()) {
            glm::vec3 emissive;
            for (unsigned int j = 0; j < material["emissiveFactor"].size(); j++) {
                emissive[j] = material["emissiveFactor"][j];
            }
            mat->emission = glm::vec4(emissive, 1.0f);
        }

        mat->shader = AssetLoader<ShaderProgram>::get("pbr");
        

        if(material.find("doubleSided") != material.end()) {
            mat->pipelineState.faceCulling.enabled = !material["doubleSided"];
        } else {
            mat->pipelineState.faceCulling.enabled = true;
        }
        

        mat->pipelineState.depthTesting.enabled = true;
        mat->pipelineState.depthTesting.function = GL_LEQUAL;

        // add all the lights in the world
        std::unordered_map<std::string, Light*>lights = AssetLoader<Light>::getAll();

        for (auto& [name, light] : lights) {
            mat->lights.push_back(light);
        }

        materials.push_back(mat);
    }
}

void Model::loadModel(std::string path) {

    this->path = path;

    std::string text = get_file_contents(this->path);

    // Load the JSON file
    JSON = json::parse(text);
    std::cout << "Loaded JSON file: " << this->path << '\n';

    // Get data from bin file
    data = getData();
    std::cout << "Loaded binary data from: " << JSON["buffers"][0]["uri"].get<std::string>() << '\n';

    // Load the textures
    loadTextures();

    // Load the materials
    loadMaterials();

    traverseNode(0, glm::mat4(1.0f));
}

void Model::traverseNode(unsigned int nextNode, glm::mat4 matrix) {
    // Get the node data
    json node = JSON["nodes"][nextNode];

    // Get translation
    glm::vec3 translation = glm::vec3(0.0f, 0.0f, 0.0f);
    if (node.find("translation") != node.end()) {
        float transValues[3];
        for (unsigned int i = 0; i < node["translation"].size(); i++)
            transValues[i] = (node["translation"][i]);
        translation = glm::make_vec3(transValues);
    }

    // Get quaternion rotation
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (node.find("rotation") != node.end()) {
        float rotValues[4];
        for (unsigned int i = 0; i < node["rotation"].size(); i++)
            rotValues[i] = (node["rotation"][i]);
        rotation = glm::make_quat(rotValues);
    }

    // Get scale
    glm::vec3 scale = glm::vec3(1.0f, 1.0f, 1.0f);
    if (node.find("scale") != node.end()) {
        float scaleValues[3];
        for (unsigned int i = 0; i < node["scale"].size(); i++)
            scaleValues[i] = (node["scale"][i]);
        scale = glm::make_vec3(scaleValues);
    }

    // Get matrix
    glm::mat4 matNode = glm::mat4(1.0f);
    if (node.find("matrix") != node.end()) {
        float matValues[16];
        for (unsigned int i = 0; i < node["matrix"].size(); i++)
            matValues[i] = (node["matrix"][i]);
        matNode = glm::make_mat4(matValues);
    }


    // Initialize matrices
    glm::mat4 trans = glm::mat4(1.0f);
    glm::mat4 rot = glm::mat4(1.0f);
    glm::mat4 sca = glm::mat4(1.0f);

    // Use translation, rotation, and scale to change the initialized matrices
    trans = glm::translate(trans, translation);
    rot = glm::mat4_cast(rotation);
    sca = glm::scale(sca, scale);

    // Combine the matrices
    glm::mat4 matNextNode = matrix * matNode * trans * rot * sca;

    // Check if the node contains a mesh and if it does load it
    if (node.find("mesh") != node.end()) {
        translationsMeshes.push_back(translation);
        rotationsMeshes.push_back(rotation);
        scalesMeshes.push_back(scale);
        matricesMeshes.push_back(matNextNode);
        loadMesh(node["mesh"]);
    }

    // Check if the node has children, and if it does, apply this function to them with the matNextNode
    if (node.find("children") != node.end()) {
        for (unsigned int i = 0; i < node["children"].size(); i++)
            traverseNode(node["children"][i], matNextNode);
    }
}

std::vector<float> Model::getFloats(json accessor)
{
	std::vector<float> floatVec;

	// Get properties from the accessor
	unsigned int buffViewInd = accessor.value("bufferView", 1);
	unsigned int count = accessor["count"];
	unsigned int accByteOffset = accessor.value("byteOffset", 0);
	std::string type = accessor["type"];

	// Get properties from the bufferView
	json bufferView = JSON["bufferViews"][buffViewInd];
	unsigned int byteOffset = bufferView["byteOffset"];

	// Interpret the type and store it into numPerVert
	unsigned int numPerVert;
	if (type == "SCALAR") numPerVert = 1;
	else if (type == "VEC2") numPerVert = 2;
	else if (type == "VEC3") numPerVert = 3;
	else if (type == "VEC4") numPerVert = 4;
	else throw std::invalid_argument("Type is invalid (not SCALAR, VEC2, VEC3, or VEC4)");

	// Go over all the bytes in the data at the correct place using the properties from above
	unsigned int beginningOfData = byteOffset + accByteOffset;
	unsigned int lengthOfData = count * 4 * numPerVert;
	for (unsigned int i = beginningOfData; i < beginningOfData + lengthOfData; i += 4)
	{
		unsigned char bytes[] = { data[i], data[i + 1], data[i + 2], data[i + 3] };
		float value;
		std::memcpy(&value, bytes, sizeof(float));
		floatVec.push_back(value);
	}

	return floatVec;
}

std::vector<GLuint> Model::getIndices(json accessor)
{
    std::vector<GLuint> indices;

    // Verify accessor has required fields
    if (!accessor.contains("count") || !accessor.contains("componentType")) {
        std::cerr << "ERROR: Accessor missing required fields for indices" << std::endl;
        return indices;
    }

    // Get properties from the accessor
    unsigned int buffViewInd = accessor.value("bufferView", 0);
    unsigned int count = accessor["count"];
    unsigned int accByteOffset = accessor.value("byteOffset", 0);
    unsigned int componentType = accessor["componentType"];

    // Check bufferView exists
    if (buffViewInd >= JSON["bufferViews"].size()) {
        std::cerr << "ERROR: Invalid bufferView index: " << buffViewInd << std::endl;
        return indices;
    }

    // Get properties from the bufferView
    json bufferView = JSON["bufferViews"][buffViewInd];
    unsigned int byteOffset = bufferView.value("byteOffset", 0);

    // Calculate beginning of data and verify it's within bounds
    unsigned int beginningOfData = byteOffset + accByteOffset;
    if (beginningOfData >= data.size()) {
        std::cerr << "ERROR: Data offset out of bounds: " << beginningOfData << " >= " << data.size() << std::endl;
        return indices;
    }

    // Process based on component type
    if (componentType == 5125) // UNSIGNED_INT (4 bytes)
    {
        // Check if we have enough data
        if (beginningOfData + count * 4 > data.size()) {
            std::cerr << "ERROR: Buffer overrun detected for UNSIGNED_INT indices" << std::endl;
            count = (data.size() - beginningOfData) / 4; // Adjust count to prevent overrun
        }

        for (unsigned int i = beginningOfData; i < beginningOfData + count * 4; i += 4)
        {
            unsigned char bytes[] = { data[i], data[i + 1], data[i + 2], data[i + 3] };
            unsigned int value;
            std::memcpy(&value, bytes, sizeof(unsigned int));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5123) // UNSIGNED_SHORT (2 bytes)
    {
        // Check if we have enough data
        if (beginningOfData + count * 2 > data.size()) {
            std::cerr << "ERROR: Buffer overrun detected for UNSIGNED_SHORT indices" << std::endl;
            count = (data.size() - beginningOfData) / 2; // Adjust count to prevent overrun
        }

        for (unsigned int i = beginningOfData; i < beginningOfData + count * 2; i += 2)
        {
            unsigned char bytes[] = { data[i], data[i + 1] };
            unsigned short value;
            std::memcpy(&value, bytes, sizeof(unsigned short));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5122) // SHORT (2 bytes)
    {
        // Check if we have enough data
        if (beginningOfData + count * 2 > data.size()) {
            std::cerr << "ERROR: Buffer overrun detected for SHORT indices" << std::endl;
            count = (data.size() - beginningOfData) / 2; // Adjust count to prevent overrun
        }

        for (unsigned int i = beginningOfData; i < beginningOfData + count * 2; i += 2)
        {
            unsigned char bytes[] = { data[i], data[i + 1] };
            short value;
            std::memcpy(&value, bytes, sizeof(short));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5126) // FLOAT (4 bytes)
    {
        // Check if we have enough data
        if (beginningOfData + count * 4 > data.size()) {
            std::cerr << "ERROR: Buffer overrun detected for FLOAT indices" << std::endl;
            count = (data.size() - beginningOfData) / 4; // Adjust count to prevent overrun
        }

        // Fix: Use beginningOfData + count*4 instead of beginningOfData * 4
        for (unsigned int i = beginningOfData; i < beginningOfData + count * 4; i += 4)
        {
            unsigned char bytes[] = { data[i], data[i + 1], data[i + 2], data[i + 3] };
            float value;
            std::memcpy(&value, bytes, sizeof(float));
            indices.push_back((GLuint)value);
        }
    }
    else if (componentType == 5121) // UNSIGNED_BYTE (1 byte)
    {
        // Check if we have enough data
        if (beginningOfData + count > data.size()) {
            std::cerr << "ERROR: Buffer overrun detected for UNSIGNED_BYTE indices" << std::endl;
            count = data.size() - beginningOfData; // Adjust count to prevent overrun
        }

        // Fix: Use beginningOfData + count instead of just beginningOfData
        for (unsigned int i = beginningOfData; i < beginningOfData + count; i++)
        {
            indices.push_back((GLuint)data[i]);
        }
    }
    else if (componentType == 5120) // BYTE (1 byte)
    {
        // Check if we have enough data
        if (beginningOfData + count > data.size()) {
            std::cerr << "ERROR: Buffer overrun detected for BYTE indices" << std::endl;
            count = data.size() - beginningOfData; // Adjust count to prevent overrun
        }

        // Fix: Use beginningOfData + count instead of just beginningOfData
        for (unsigned int i = beginningOfData; i < beginningOfData + count; i++)
        {
            indices.push_back((GLuint)(char)data[i]);
        }
    }
    else
    {
        std::cerr << "Unsupported component type: " << componentType << std::endl;
    }

    return indices;
}

std::vector<Vertex> Model::assembleVertices(std::vector<glm::vec3> positions,std::vector<glm::vec3> normals,std::vector<glm::vec2> texUVs)
{
	std::vector<Vertex> vertices;
	for (int i = 0; i < positions.size(); i++)
	{
        texUVs[i].y = 1 - texUVs[i].y;
		vertices.push_back
		(
			Vertex
			{
				positions[i],
                glm::vec4(1, 1, 1, 1),
                texUVs[i],
                normals[i]
			}
		);
	}
	return vertices;
}

std::vector<glm::vec2> Model::groupFloatsVec2(std::vector<float> floatVec) {
    const unsigned int floatsPerVector = 2;

    std::vector<glm::vec2> vectors;
    for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector) {
        vectors.push_back(glm::vec2(0, 0));

        for (unsigned int j = 0; j < floatsPerVector; j++) {
            vectors.back()[j] = floatVec[i + j];
        }
    }
    return vectors;
}

std::vector<glm::vec3> Model::groupFloatsVec3(std::vector<float> floatVec) {
    const unsigned int floatsPerVector = 3;

    std::vector<glm::vec3> vectors;
    for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector) {
        vectors.push_back(glm::vec3(0, 0, 0));

        for (unsigned int j = 0; j < floatsPerVector; j++) {
            vectors.back()[j] = floatVec[i + j];
        }
    }
    return vectors;
}

std::vector<glm::vec4> Model::groupFloatsVec4(std::vector<float> floatVec) {
    const unsigned int floatsPerVector = 4;

    std::vector<glm::vec4> vectors;
    for (unsigned int i = 0; i < floatVec.size(); i += floatsPerVector) {
        vectors.push_back(glm::vec4(0, 0, 0, 0));

        for (unsigned int j = 0; j < floatsPerVector; j++) {
            vectors.back()[j] = floatVec[i + j];
        }
    }
    return vectors;
}

} // namespace our