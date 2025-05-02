#version 330 core
in vec2 TexCoords;
out vec4 FragColor;
uniform bool useTexture;

uniform sampler2D text;
uniform vec4 textColor;

void main() {
    if (!useTexture) {
        FragColor = textColor;
    } else {
        float alpha = texture(text, TexCoords).r;
        FragColor = vec4(textColor.r, textColor.g, textColor.b, textColor.a * alpha);
    }
}
