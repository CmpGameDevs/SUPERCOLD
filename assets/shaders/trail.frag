#version 330 core
out vec4 FragColor;

uniform vec4 trailColor;
in float vSegmentRatio;

void main() {
    float alpha = max(0.0, 1.0 - vSegmentRatio * 2.0);
    FragColor = vec4(trailColor.rgb, trailColor.a * alpha);
}