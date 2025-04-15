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


struct SpotLight {
    // These defines the colors and intensities of the light.
    vec3 diffuse;
    vec3 specular;
    vec3 ambient;

    // Spot lights only has a position and a direction.
    // Note that unlike directional lights, the direction here is not the direction of the light relative to the pixel; its the direction of the spot light's cone axis.
    vec3 position, direction;
    // The attenuation is used to control how the light dims out as we go further from it.
    float attenuation_constant;
    float attenuation_linear;
    float attenuation_quadratic;
    // The angles define the spot light cone shape.
    // Inside the inner cone, the light intensity is full. Outside the outer angle, the light intensity is 0.
    // In between we use a smooth step to compute the light intensity.
    float inner_angle, outer_angle;
};

// Material properties
uniform vec3 materialAmbient;    // Ambient reflectivity
uniform vec3 materialDiffuse;    // Diffuse reflectivity
uniform vec3 materialSpecular;   // Specular reflectivity
uniform float materialShininess; // Shininess factor

// Directional light properties
uniform SpotLight light;

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

    vec3 lightDir = fs_in.FragPos - light.position;
    float distance = length(lightDir);
    lightDir = normalize(lightDir);

    // Calculate the attenuation factor based on the light distance from the pixel.
    float attenuation = 1.0f / (light.attenuation_constant +
                                light.attenuation_linear * distance +
                                light.attenuation_quadratic * distance * distance);


    // Then we calculate the angle between the pixel and the cone axis.
    float angle = acos(dot(light.direction, lightDir));
    // And we calculate the attenuation based on the angle.
    float angle_attenuation = smoothstep(light.outer_angle, light.inner_angle, angle);

    // Calculate ambient, diffuse, and specular components
    vec3 ambient = light.ambient * materialAmbient;
    float diff = calculate_lambert(norm, lightDir);
    vec3 diffuse = diff * light.diffuse * materialDiffuse;
    float spec = calculate_phong(norm, lightDir, viewDir, materialShininess);
    vec3 specular = spec * light.specular * materialSpecular;

    // Combine all components and apply the fragment color
    vec3 lighting = (diffuse + specular) * attenuation * angle_attenuation + ambient;
    FragColor = fs_in.Color * vec4(lighting, 1.0);
}

