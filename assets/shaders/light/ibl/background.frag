#version 330 core
layout (location = 0) out vec4 FragColor; // regular output
layout (location = 1) out vec4 BloomColor; // output to be used by bloom shader

#define GREYSCALE_WEIGHT_VECTOR vec3(0.2126, 0.7152, 0.0722)

in vec3 WorldPos;

uniform samplerCube environmentMap;
uniform float bloomBrightnessCutoff;

void main()
{		
    vec3 envColor = texture(environmentMap, WorldPos).rgb;
    
    // HDR tonemap and gamma correct
    envColor = envColor / (envColor + vec3(1.0));
    envColor = pow(envColor, vec3(1.0/2.2)); 
    
    FragColor = vec4(envColor, 1.0);

    // bloom color output
	// use greyscale conversion here because not all colors are equally "bright"
	float greyscaleBrightness = dot(FragColor.rgb, GREYSCALE_WEIGHT_VECTOR);
	BloomColor = greyscaleBrightness > bloomBrightnessCutoff ? FragColor : vec4(0.0, 0.0, 0.0, 1.0);

}