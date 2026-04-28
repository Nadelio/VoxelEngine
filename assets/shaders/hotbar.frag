#version 330 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uAtlas;
uniform vec4 uColor;
uniform int uUseTexture;

void main() {
    if (uUseTexture != 0) {
        FragColor = texture(uAtlas, vUV) * uColor;
    } else {
        FragColor = uColor;
    }
}
