#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTextureCoordinates;

out vec2 textureCoordinates;
out vec3 worldCoordinates;
out vec3 tangent;
out vec3 bitangent;
out vec3 normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
	worldCoordinates = vec3(model * vec4(aPos, 1.0f));
	textureCoordinates = aTextureCoordinates;

	mat3 normalMatrix = transpose(inverse(mat3(model)));

	normal = normalize(normalMatrix * aNormal);
	
	gl_Position = projection * view * vec4(worldCoordinates, 1.0f);
}