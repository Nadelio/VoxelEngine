#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uSkin;

void main() {
    vec4 color = texture(uSkin, vUV);
    if (color.a < 0.1) discard;
    FragColor = color;
}
