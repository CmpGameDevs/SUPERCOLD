#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTextureCoordinates;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out vec2 textureCoordinates;
out vec3 worldCoordinates;
out vec3 tangent;
out vec3 bitangent;
out vec3 normal;

uniform mat4 transform;
// uniform mat4 view;
// uniform mat4 projection;

void main() {
	worldCoordinates = (transform * vec4(aPos, 1.0f)).xyz;
	gl_Position = transform * vec4(aPos, 1.0f);
	textureCoordinates = aTextureCoordinates;

	mat3 normalMatrix = transpose(inverse(mat3(transform)));

	tangent = normalize(normalMatrix * aTangent);
	bitangent = normalize(normalMatrix * aBitangent);
	normal = normalize(normalMatrix * aNormal);
}