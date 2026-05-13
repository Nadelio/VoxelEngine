#version 330 core
in vec2 vUV;
in vec2 vTileOrigin;
in vec4 vShadowCoord;
out vec4 FragColor;

uniform sampler2D uAtlas;
uniform sampler2D uShadowMap;
uniform vec2 uTileSize;

float SampleShadow(vec4 shadowCoord) {
    vec3 projCoords = shadowCoord.xyz / max(shadowCoord.w, 0.0001);
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    float shadow = 0.0;
    float bias = 0.0015;
    vec2 texelSize = 1.0 / vec2(textureSize(uShadowMap, 0));
    for(int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float closestDepth = texture(uShadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > closestDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

void main() {
    // Wrap vUV within the tile's bounds so merged quads tile the texture correctly.
    vec2 tileUV = mod(vUV - vTileOrigin, uTileSize) + vTileOrigin;
    vec4 color = texture(uAtlas, tileUV);
    if(color.a < 0.1) {
        discard;
    }
    float shadow = SampleShadow(vShadowCoord);
    color.rgb *= mix(1.0, 0.55, shadow);
    FragColor = color;
}
