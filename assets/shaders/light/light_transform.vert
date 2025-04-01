#version 330 core
layout(location = 0) in vec3 aPos;    // Vertex position
layout(location = 1) in vec4 aColor;  // Vertex color
layout(location = 3) in vec3 aNormal; // Vertex normal


out varyings {
    vec3 FragPos;  // Position of the fragment in world space
    vec3 Normal;   // Normal vector in world space
    vec3 ViewDir;  // Direction from fragment to viewer
    vec4 Color;  // Color of the fragment
} vs_out;


uniform mat4 transform;
uniform vec3 viewPos; // Projection matrix

void main()
{
    // Transform vertex position to world space
    vs_out.FragPos = vec3(transform * vec4(aPos, 1.0));
    
    // Transform normal to world space
    vs_out.Normal = mat3(transpose(inverse(transform))) * aNormal; 
    
    // Calculate view direction
    vs_out.ViewDir = normalize(viewPos - vs_out.FragPos);

    // Output the final vertex position
    gl_Position = transform * vec4(aPos, 1.0);

    // Pass the color to the fragment shader
    vs_out.Color = aColor;
}
