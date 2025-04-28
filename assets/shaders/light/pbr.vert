#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 2) in vec2 aTextureCoordinates;
layout (location = 3) in vec3 aNormal;
layout (location = 4) in ivec4 boneIndices;
layout (location = 5) in vec4 boneWeights;

out vec2 textureCoordinates;
out vec3 worldCoordinates;
out vec3 normal;
out vec4 boneWeightsOut;
flat out ivec4 boneIndicesOut;

const int MAX_BONES = 200;
const int MAX_BONE_INFLUENCE = 4;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 boneTransforms[MAX_BONES];

void main() {
    vec4 totalPosition = vec4(0.0f);
    vec3 totalNormal = vec3(0.0f);
    
    // Apply bone transformations
    for (int i = 0; i < MAX_BONE_INFLUENCE; i++) {
        if (boneIndices[i] < 0) {
            continue; // Skip invalid bone indices
        }
        if (boneIndices[i] >= MAX_BONES) {
            // Handle out-of-bounds bone index
            totalPosition = vec4(aPos, 1.0f);
            totalNormal = aNormal;
            break;
        }
        
        // Transform position
        vec4 localPosition = boneTransforms[boneIndices[i]] * vec4(aPos, 1.0f);
        totalPosition += boneWeights[i] * localPosition;
        
        // Transform normal
        vec3 localNormal = mat3(boneTransforms[boneIndices[i]]) * aNormal;
        totalNormal += boneWeights[i] * localNormal;
    }

    // If no bone influence, use original position and normal
    if (totalPosition == vec4(0.0f)) {
        totalPosition = vec4(aPos, 1.0f);
        totalNormal = aNormal;
    }

    // Transform to world space
    worldCoordinates = vec3(model * totalPosition);
    textureCoordinates = aTextureCoordinates;

    // Transform normal to world space
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    normal = normalize(normalMatrix * normalize(totalNormal));

    // Final position
    gl_Position = projection * view * vec4(worldCoordinates, 1.0f);

    // send out the bone indices
    boneIndicesOut = boneIndices;
    boneWeightsOut = boneWeights;
}


    // vec4 transformedPosition = vec4(0.0);

		// transformedPosition = vec4(aPos, 1.0);

    // worldCoordinates = vec3(model * transformedPosition);
    // textureCoordinates = aTextureCoordinates;

    // mat3 normalMatrix = transpose(inverse(mat3(model)));
    // normal = normalize(normalMatrix * aNormal);

    // gl_Position = projection * view * vec4(worldCoordinates, 1.0f);

// void main() {
// 	// Initialize transformed position and normal
// 	vec4 transformedPosition = vec4(0.0);
// 	vec3 transformedNormal = vec3(0.0);

// 	// Normalize bone weights
// 	vec4 normalizedWeights = boneWeights;
// 	float weightSum = dot(normalizedWeights, vec4(1.0));
// 	if (weightSum > 0.0) {
// 		normalizedWeights /= weightSum;
// 	} else {
// 		normalizedWeights = vec4(1.0, 0.0, 0.0, 0.0); // Default to first bone if no weights
// 	}

// 	// Apply bone transformations
// 	for (int i = 0; i < 4; i++) {
// 		if (normalizedWeights[i] > 0.0) {
// 			// Transform position
// 			vec4 localPos = boneTransforms[boneIndices[i]] * vec4(aPos, 1.0);
// 			transformedPosition += normalizedWeights[i] * localPos;
			
// 			// Transform normal
// 			vec3 localNormal = mat3(boneTransforms[boneIndices[i]]) * aNormal;
// 			transformedNormal += normalizedWeights[i] * localNormal;
// 		}
// 	}

// 	// If no bone influence, use original position and normal
// 	if (transformedPosition == vec4(0.0)) {
// 		transformedPosition = vec4(aPos, 1.0);
// 		transformedNormal = aNormal;
// 	}

// 	// Transform to world space
// 	worldCoordinates = vec3(model * transformedPosition);
// 	textureCoordinates = aTextureCoordinates;

// 	// Transform normal to world space
// 	mat3 normalMatrix = transpose(inverse(mat3(model)));
// 	normal = normalize(normalMatrix * normalize(transformedNormal));

// 	// Final position
// 	gl_Position = projection * view * vec4(worldCoordinates, 1.0f);
// 	boneIndicesOut = boneIndices;
// 	boneCount = 0;
// 	for (int i = 0; i < 4; i++) {
// 		if (normalizedWeights[i] > 0.0) {
// 			boneCount++;
// 		}
// 	}
// 	boneCount = min(boneCount, 4); // Limit to 4 bones for the shader
// }


// #version 330 core

// layout (location = 0) in vec3 aPos;
// layout (location = 2) in vec2 aTextureCoordinates;
// layout (location = 3) in vec3 aNormal;
// layout (location = 4) in ivec4 boneIndices;
// layout (location = 5) in vec4 boneWeights;

// out vec2 textureCoordinates;
// out vec3 worldCoordinates;
// out vec3 normal;

// uniform mat4 model;
// uniform mat4 view;
// uniform mat4 projection;
// uniform mat4 boneTransforms[100]; // Assuming a maximum of 100 bones

// void main() {
//     vec4 transformedPosition = vec4(0.0);

//     // Apply bone transformations
//     for (int i = 0; i < 4; i++) {
//         if (boneWeights[i] > 0.0) {
//             transformedPosition += boneWeights[i] * (boneTransforms[boneIndices[i]] * vec4(aPos, 1.0));
//         }
//     }

//     worldCoordinates = vec3(model * transformedPosition);
//     textureCoordinates = aTextureCoordinates;

//     mat3 normalMatrix = transpose(inverse(mat3(model)));
//     normal = normalize(normalMatrix * aNormal);

//     gl_Position = projection * view * vec4(worldCoordinates, 1.0f);
// }