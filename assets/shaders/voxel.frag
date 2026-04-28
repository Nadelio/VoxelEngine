#version 330 core
in vec2 vUV;
in vec2 vTileOrigin;
out vec4 FragColor;

uniform sampler2D uAtlas;
uniform vec2 uTileSize;

void main() {
    // Wrap vUV within the tile's bounds so merged quads tile the texture correctly.
    vec2 tileUV = mod(vUV - vTileOrigin, uTileSize) + vTileOrigin;
    FragColor = texture(uAtlas, tileUV);
}
