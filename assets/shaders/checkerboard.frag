#version 330 core

out vec4 frag_color;

// In this shader, we want to draw a checkboard where the size of each tile is (size x size).
// The color of the top-left most tile should be "colors[0]" and the 2 tiles adjacent to it
// should have the color "colors[1]".

//TODO: (Req 1) Finish this shader.

uniform int size = 32;
uniform vec3 colors[2];

void main(){
    // NOTE: this line gets in which coordinate we are in the screen
    ivec2 coord = ivec2(gl_FragCoord.xy) / size;

    // NOTE: this line checks if the sum of the coordinates is even or odd
    // which basically is how you know you are black or which in chess
    int checker = (coord.x + coord.y) % 2;

    frag_color = vec4(colors[checker], 1.0);
}