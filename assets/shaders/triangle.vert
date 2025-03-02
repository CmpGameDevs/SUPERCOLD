#version 330

// This vertex shader should be used to render a triangle whose normalized device coordinates are:
// (-0.5, -0.5, 0.0), ( 0.5, -0.5, 0.0), ( 0.0,  0.5, 0.0)
// And it also should send the vertex color as a varying to the fragment shader where the colors are (in order):
// (1.0, 0.0, 0.0), (0.0, 1.0, 0.0), (0.0, 0.0, 1.0)

out Varyings {
    vec3 color;
} vs_out;

// Currently, the triangle is always in the same position, but we don't want that.
// So two uniforms should be added: translation (vec2) and scale (vec2).
// Each vertex "v" should be transformed to be "scale * v + translation".
// The default value for "translation" is (0.0, 0.0) and for "scale" is (1.0, 1.0).

//TODO: (Req 1) Finish this shader
// NOTE default value means no translation
uniform vec2 translation = vec2(0.0, 0.0);
// NOTE default value means no scaling
uniform vec2 scale = vec2(1.0, 1.0);

void main(){
    // NOTE: positions are vec2 as z is always 0.0 here
    vec2 positions[3] = vec2[3](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.0,  0.5)
    );

    vec3 colors[3] = vec3[3](
        vec3(1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, 0.0, 1.0)
    );

    gl_Position = vec4(scale * positions[gl_VertexID] + translation, 0.0, 1.0);
    vs_out.color = colors[gl_VertexID];
}