#version 330 core

// Inputs from the vertex shader
in varyings{
    vec3 FragPos;  // Position of the fragment in world space
    vec3 Normal;   // Normal vector in world space
    vec3 ViewDir;  // Direction from fragment to viewer
    vec4 Color;   // Color of the fragment
} fs_in;
// Output color
out vec4 FragColor;

struct DirectionalLight {
    // These defines the colors and intensities of the light.
    vec3 diffuse;
    vec3 specular;
    vec3 ambient;
    // Directional light are only defined by a direction. (It has no position).
    vec3 direction;
};

// Material properties
uniform vec3 materialAmbient;    // Ambient reflectivity
uniform vec3 materialDiffuse;    // Diffuse reflectivity
uniform vec3 materialSpecular;   // Specular reflectivity
uniform float materialShininess; // Shininess factor

// Directional light properties
uniform DirectionalLight light;

float calculate_lambert(vec3 normal, vec3 light_direction){
        return max(0.0f, dot(normal, -light_direction));
}

    // This will be used to compute the phong specular.
float calculate_phong(vec3 normal, vec3 light_direction, vec3 view, float shininess){
    vec3 reflected = reflect(light_direction, normal);
    return pow(max(0.0f, dot(view, reflected)), shininess);
}

void main()
{
    // Normalize the normal and light direction vectors
    vec3 norm = normalize(fs_in.Normal);
    vec3 viewDir = normalize(fs_in.ViewDir);

    // Calculate ambient, diffuse, and specular components
    vec3 ambient = light.ambient * materialAmbient;
    float diff = calculate_lambert(norm, light.direction);
    vec3 diffuse = diff * light.diffuse * materialDiffuse;
    float spec = calculate_phong(norm, light.direction, viewDir, materialShininess);
    vec3 specular = spec * light.specular * materialSpecular;

    // Combine all components and apply the fragment color
    FragColor = vec4(ambient + diffuse + specular, 1.0) * fs_in.Color;
}

