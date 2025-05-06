#version 330 core

out vec4 FragColor;
in vec2 tex_coord;

uniform sampler2D colorTexture;
uniform sampler2D bloomTexture;

uniform sampler2D previousFrameTexture;  // For motion blur

uniform sampler2D depthTexture;   // Add depth texture sampler
uniform mat4 viewProjectionMatrix;
uniform mat4 previousViewProjectionMatrix;
uniform mat4 viewProjectionInverseMatrix;

// for freeze effect
uniform sampler2D freezeFrameTexture;
uniform bool hasFrameTexture = false;

uniform bool bloomEnabled;
uniform float bloomIntensity;
uniform bool tonemappingEnabled;
uniform float gammaCorrectionFactor;

// NOTE: Vignette effect parameters (used for freeze effect) (blue color around the edges)
uniform bool vignetteEnabled = false;
uniform float vignetteIntensity = 0.5;
uniform vec3 vignetteColor = vec3(0.0, 0.0, 0.0);
uniform vec2 screenSize = vec2(800, 600);

// Motion Blue effects
uniform bool motionBlurEnabled = false;
uniform float motionBlurStrength = 0.5;
uniform int motionBlurSamples = 5;
uniform vec2 motionDirection = vec2(1.0, 0.0);


// Apply vignette effect
vec3 applyVignette(vec3 color, vec2 uv) {
    if (!vignetteEnabled) return color;
		if (vignetteIntensity <= 0) return color;
    

    // Calculate distance from center (normalized coords)
    vec2 center = vec2(0.5, 0.5);
    float dist = length(uv - center) * 1.8; // * 2 to make it reach corners
    
    // Calculate vignette factor (1.0 at center, 0.0 at edges)
    float vignette = smoothstep(1.0, 0.9 * vignetteIntensity, dist);
    
    // Mix with vignette color based on intensity
    return mix(color, vignetteColor, (1 - vignette) * vignetteIntensity);
}

// Apply freeze effect
vec3 applyFreezeEffect(vec3 color) {
    if (!hasFrameTexture || vignetteIntensity <= 0.0 || !vignetteEnabled) return color;

    vec4 frameTex = texture(freezeFrameTexture, tex_coord);
    float alpha = frameTex.a;

    // Skip transparent pixels entirely
    if (alpha <= 0.01) return color;

    // Luminance of the frost pattern for variation
    float frostStrength = dot(frameTex.rgb, vec3(0.299, 0.587, 0.114));

    // Apply frost tint color (blueish/white) modulated by frostStrength and intensity
    vec3 frostColor = mix(color, vignetteColor, frostStrength * vignetteIntensity);

    // Add some frosty highlight 
    vec3 highlight = mix(frostColor, vec3(1.0), frostStrength * vignetteIntensity * 0.4);

    return highlight;
}


// Apply motion blur effect
vec3 applyMotionBlur(vec3 currentColor) {
    if (!motionBlurEnabled || motionBlurStrength <= 0.0) return currentColor;
    
    // Get the depth buffer value at this pixel
    float zOverW = texture(depthTexture, tex_coord).r;
    
    // Calculate viewport position
    vec4 H = vec4(tex_coord.x * 2.0 - 1.0, 
                  (1.0 - tex_coord.y) * 2.0 - 1.0,
                  zOverW,
                  1.0);
                  
    // Transform by the view-projection inverse to get world position
    vec4 D = viewProjectionInverseMatrix * H;
    vec4 worldPos = D / D.w;
    
    // Current viewport position
    vec4 currentPos = H;
    
    // Get previous frame position
    vec4 previousPos = previousViewProjectionMatrix * worldPos;
    previousPos /= previousPos.w;
    
    // Compute velocity vector
    vec2 velocity = (currentPos.xy - previousPos.xy) * motionBlurStrength * 0.5;
    
    // Accumulate samples
    vec3 color = currentColor;
    float samples = 1.0;
    
    // Sample along velocity vector
    for(int i = 1; i < motionBlurSamples; ++i) {
        vec2 offset = velocity * (float(i) / float(motionBlurSamples));
        vec2 sampleCoord = tex_coord + offset;
        
        if(sampleCoord.x >= 0.0 && sampleCoord.x <= 1.0 && 
           sampleCoord.y >= 0.0 && sampleCoord.y <= 1.0) {
            color += texture(colorTexture, sampleCoord).rgb;
            samples += 1.0;
        }
    }
    
    return color / samples;
}

void main() {
	vec3 color = texture(colorTexture, tex_coord).rgb;

	// bloom
	if (bloomEnabled) {
		vec3 bloomColor = vec3(0.0, 0.0, 0.0);
		for (int i = 0; i <= 5; i++) {
			bloomColor += textureLod(bloomTexture, tex_coord, i).rgb;
		}

		color += bloomColor * bloomIntensity;
	}

    // Apply motion blur
    // NOTE: keep it before freeze effect.
    if (motionBlurEnabled) {
        color = applyMotionBlur(color);
    }

	if (vignetteEnabled) {
		color = applyVignette(color, tex_coord); // Apply vignette effect
		color = applyFreezeEffect(color);        // Apply freeze effect
	}

	// tonemapping
	if (tonemappingEnabled) {
		// apply Reinhard tonemapping C = C / (1 + C)
		color = color / (color + vec3(1.0));
	}


	// gamma correction
	// color = pow(color, vec3(1.0 / gammaCorrectionFactor)); // gamma correction to account for monitor, raise to the (1 / 2.2)

	FragColor = vec4(color, 1.0);
}