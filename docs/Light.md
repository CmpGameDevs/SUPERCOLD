# PBR Lighting System and Image Based Lighting (IBL) in SUPERCOLD

This document explains the lighting system used in our game. We cover our PBR (Physically Based Rendering) lighting pipeline, including how we define lights, materials, and how we use image based lighting (IBL) with HDR cubemaps. Examples and code snippets are provided for clarity.

---

## 1. Overview

Our lighting system is designed with realism in mind. It consists of:
- **Light Sources:** Different light types such as directional, point, and spot lights.
- **Lit Material:** Materials that define surface properties like albedo, metallic, roughness, ambient occlusion, normal, and emissive components.
- **Image Based Lighting (IBL):** Uses HDR images and cubemaps to simulate realistic ambient and reflective lighting.
- **Texture Unit Management:** A dedicated system in which texture units are managed through the `TextureUnits` class, ensuring consistency in binding.

---

## 2. Light Class

Lights are the primary illumination sources in our scene. They exist in three different forms:

### Light Types
- **Directional Light:**  
  Uses a direction vector rather than a position.  
  *Example Property:*  
  `direction: [-0.2, -1.0, -0.3]`

- **Point Light:**  
  Defined by a position with attenuation factors showing how the light dims with distance.  
  *Attenuation Example:*
  ```json
  "attenuation": {
      "constant": 1.0,
      "linear": 0.09,
      "quadratic": 0.032
    }
  ```

- **Spot Light:**  
  Similar to point lights but with an inner and outer cone angle to define the light's spread.

  ```json
  "spot_angle": {
        "inner": 0.785398163, // pi/4
        "outer": 1.57079633 // pi/2
    }
  ```

Each light also has an `enabled` flag to turn it on or off.

### Deserialization Example (C++)

```cpp
void Light::deserialize(const nlohmann::json& data){
    if(!data.is_object()) return;
    
    // Determine the light type
    std::string type = data.value("type", "");
    if(type == "directional"){
        this->type = LightType::DIRECTIONAL;
    } else if(type == "point"){
        this->type = LightType::POINT;
    } else if(type == "spot"){
        this->type = LightType::SPOT;
    } else{
        std::cerr << "Unknown light type: " << type << std::endl;
        return;
    }
    
    // Read basic properties
    enabled = data.value("enabled", true);
    color = data.value("color", glm::vec3{1.0f});
    position = data.value("position", glm::vec3{0.0f});
    direction = data.value("direction", glm::vec3{0.0f});
    // Read additional properties for attenuation and spot angles if provided...
}
```

### JSON Example for a Light
```json
{
  "type": "point",
  "enabled": true,
  "color": [1.0, 1.0, 1.0],
  "position": [0.0, 5.0, 0.0],
  "attenuation": {
      "constant": 1.0,
      "linear": 0.09,
      "quadratic": 0.032
  }
}
```

---

## 3. Lit Material Class

The `LitMaterial` class defines how a material reacts to lighting. It holds several key attributes:

### Material Attributes

- **Albedo:** Base color of the material.
- **Metallic:** Degree to which the material behaves like a metal.
- **Roughness:** Surface smoothness that affects specular reflections.
- **Ambient Occlusion:** How ambient light is occluded by the object.
- **Normal Mapping:** For adding surface detail.
- **Emissive:** For self-illumination of the surface.

These values can be provided as constants or loaded from textures if the corresponding flag is set in the JSON configuration.

### Material Setup Example (C++)

```cpp
void LitMaterial::setup() const {
    // Base setup from TintedMaterial, binding common values
    TintedMaterial::setup();

    // Send material properties to the shader
    shader->set("material.useTextureAlbedo", useTextureAlbedo);
    shader->set("material.albedo", albedo);
    shader->set("material.metallic", metallic);
    shader->set("material.roughness", roughness);
    shader->set("material.ambientOcclusion", ambientOcclusion);
    shader->set("material.emission", emission);
    
    // Bind IBL textures
    shader->set("irradianceMap", our::TextureUnits::TEXTURE_UNIT_IRRADIANCE);
    shader->set("prefilterMap", our::TextureUnits::TEXTURE_UNIT_PREFILTER);
    shader->set("brdfLUT", our::TextureUnits::TEXTURE_UNIT_BRDF);
    
    // Set the number of active lights
    shader->set("lightCount", static_cast<int>(lights.size()));
    setupLight();
    
    // Bind the albedo texture if it is used
    if (useTextureAlbedo) {
        glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_ALBEDO);
        textureAlbedo->bind();
        shader->set("material.textureAlbedo", our::TextureUnits::TEXTURE_UNIT_ALBEDO);
    }
    // Repeat binding for metallic, roughness, normal, ambient occlusion, and emissive textures...
}
```

### Sending Light Data with Materials

Within `setupLight()`, each active light's properties are sent to the shader:

```cpp
void LitMaterial::setupLight() const {
    int light_index = 0;
    for (const auto &light : lights) {
        if (light->enabled) {
            std::string prefix = "lights[" + std::to_string(light_index) + "].";
            shader->set(prefix + "color", light->color);
            
            // For directional lights, send the direction
            if (light->type == our::LightType::DIRECTIONAL) {
                shader->set(prefix + "direction", glm::normalize(light->direction));
            }
            // For point and spot lights, send position and attenuation
            if (light->type == our::LightType::POINT || light->type == our::LightType::SPOT) {
                shader->set(prefix + "position", light->position);
                shader->set(prefix + "attenuation_constant", light->attenuation.constant);
                shader->set(prefix + "attenuation_linear", light->attenuation.linear);
                shader->set(prefix + "attenuation_quadratic", light->attenuation.quadratic);
            }
            // For spot lights, send inner and outer cone angles
            if (light->type == our::LightType::SPOT) {
                shader->set(prefix + "inner_angle", light->spot_angle.inner);
                shader->set(prefix + "outer_angle", light->spot_angle.outer);
            }
            light_index++;
        }
    }
}
```

---

## 4. Image Based Lighting (IBL)

IBL enhances realism by using the surrounding environment as part of the lighting computation. This is achieved by:

### HDR and Equirectangular Images

- **HDR (High Dynamic Range):** Allows fragment colors to exceed the 0.0 to 1.0 range, providing a wider palette.
- **Equirectangular Images:** Panoramic images that map 360° horizontally and 180° vertically onto a flat image. These are used as the source images for creating cubemaps.

### Cubemap Generation

Cubemaps are created by rendering the environment onto the six faces of a cube. This process uses framebuffers and renderbuffers.

#### Cubemap Buffer Example (C++)

```cpp
void CubeMapBuffer::setupFrameBuffer() {
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    // Additional setup will attach color textures during conversion.
}

void CubeMapBuffer::setupRenderBuffer() {
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size.x, size.y);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);
}
```

### HDR System Setup Example

The HDR system converts the equirectangular HDR image into environment cubemaps used in IBL in addition to other cubemaps like irradiance and prefiltering:
```cpp
void HDRSystem::setup() {
    if (!enable) return;

    // Setup cube map framebuffer and renderbuffer
    cubeMapBuffer->size = { 512, 512 };
    cubeMapBuffer->setupFrameBuffer();
    cubeMapBuffer->setupRenderBuffer();

    // Convert the HDR equirectangular image into a cubemap
    equirectangularCubeMap->convertToCubeMap(our::TextureUnits::TEXTURE_UNIT_HDR, captureProjection, captureViews);

    // Generate mipmaps for prefiltered cubemaps to simulate specular reflections
    envCubeMap->generateMipmaps();

    // Additional cubemap passes for irradiance and prefiltering occur here...
}
```

---

## 5. Managing Texture Units

The `TextureUnits` class prevents texture unit conflicts by defining a constant for each texture type.  
Example usage:
```cpp
glActiveTexture(GL_TEXTURE0 + our::TextureUnits::TEXTURE_UNIT_ALBEDO);
```
This approach ensures that the texture bindings remain consistent throughout the rendering process.

---

## 6. Asset JSON Example

Materials and lights are defined in asset JSON files. For instance:


```json
"assets":{
    "materials": {
        "rusted_iron":{
            "type": "lit",
            "shader": "pbr",
            "pipelineState": {
                "faceCulling":{
                    "enabled": false
                },
                "depthTesting":{
                    "enabled": true,
                    "function": "GL_LEQUAL"
                }
            },
            "lights": ["sun", "lamp"],
            "tint": [1, 1, 1, 1],
            "texture": "rusted_albedo",
            "sampler": "default",
            "useTextureAlbedo": true,
            "useTextureNormal": true,
            "useTextureMetallic": true,
            "useTextureRoughness": true,
            "useTextureAmbientOcclusion": true,
            "useTextureEmissive": false,
            "albedo": [0.75, 0.75, 0.75],
            "roughness": 0.5,
            "metallic": 0.85,
            "emissive": [0, 0, 0],
            "ambientOcclusion": 1,
            "textureAlbedo": "rusted_albedo",
            "textureNormal": "rusted_normal",
            "textureMetallic": "rusted_metallic",
            "textureRoughness": "rusted_roughness",
            "textureAmbientOcclusion": "rusted_ambientOcclusion",
            "textureEmissive": "rusted_ambientOcclusion"
        },
    },
    "lights": {
        "sun":{
            "type": "point",
            "enabled": true,    
            "color": [300, 300 , 300],
            "position": [-10.0,  10.0, 10.0],
            "attenuation": {
                "constant": 0,
                "linear": 0,
                "quadratic": 1
            }
        },
        "lamp":{
            "type": "point",
            "enabled": true,    
            "color": [300, 300 , 300],
            "position": [10.0,  -10.0, 10.0],
            "attenuation": {
                "constant": 0,
                "linear": 0,
                "quadratic": 1
            }
        },
    }
}
```
As you can see you can also specify if you want to use textures or not using "useTextureAlbedo" attribute etc.

To use lit materials:
1. Define your light sources in the assets.
2. Reference these lights in the material’s JSON by adding a vector of light names.
3. The system will then automatically bind these active lights to the shader during rendering.

Please use the predefined tests located in ./config/light-test/test-pbr-{0 to 3}. Note that only these tests are active; the other tests in the light test folder have been disabled and should not be run.

---

## 7. Conclusion

Our PBR lighting system consists of:
- A flexible **Light** class for directional, point, and spot lighting.
- The **LitMaterial** class, which manages material properties, texture bindings, and light setup.
- An **IBL pipeline** that converts HDR equirectangular images to cubemaps (environment, irradiance, and prefiltered).
- A centralized **HDR System** to manage high dynamic range image processing.
- Consistent **Texture Unit** management to simplify resource binding.


## Resources

Explore these links for more insights and additional support:

- [Basic PBR Lighting](https://learnopengl.com/PBR/Lighting)  
    A basic introduction to PBR lighting concepts and implementation.

- [IBL](https://learnopengl.com/PBR/IBL/Diffuse-irradiance)  
    A comprehensive guide on how to implement image based lighting.

- [Specular-IBL](https://learnopengl.com/PBR/IBL/Specular-IBL)  
    Further reading on the specular IBL implementation.

- [3D Viewer](https://www.3dviewer.net/index.html)  
    A web-based 3D viewer for testing and visualizing your models.

- [PBR Materials](https://freepbr.com/c/base-metals/)  
    Free PBR textures and resources for your projects.