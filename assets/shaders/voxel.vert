#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec2 aTileOrigin;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;
out vec2 vUV;
out vec2 vTileOrigin;
out vec4 vShadowCoord;

void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUV = aUV;
    vTileOrigin = aTileOrigin;
    vShadowCoord = uLightSpaceMatrix * worldPos;
}
