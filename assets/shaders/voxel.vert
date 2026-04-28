#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec2 aTileOrigin;

uniform mat4 uMVP;
out vec2 vUV;
out vec2 vTileOrigin;

void main() {
    gl_Position = uMVP * vec4(aPos, 1.0);
    vUV = aUV;
    vTileOrigin = aTileOrigin;
}
