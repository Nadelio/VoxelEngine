#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aJoints;
layout(location = 3) in vec4 aWeights;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uLightSpaceMatrix;
uniform int uUseSkinning;
uniform int uJointCount;
uniform mat4 uJointMatrices[64];
out vec2 vUV;
out vec4 vShadowCoord;
out vec3 vWorldPos;

void main() {
    vec4 position = vec4(aPos, 1.0);
    if(uUseSkinning != 0) {
        mat4 skin = mat4(0.0);
        int j0 = clamp(int(aJoints.x + 0.5), 0, max(uJointCount - 1, 0));
        int j1 = clamp(int(aJoints.y + 0.5), 0, max(uJointCount - 1, 0));
        int j2 = clamp(int(aJoints.z + 0.5), 0, max(uJointCount - 1, 0));
        int j3 = clamp(int(aJoints.w + 0.5), 0, max(uJointCount - 1, 0));
        skin += uJointMatrices[j0] * aWeights.x;
        skin += uJointMatrices[j1] * aWeights.y;
        skin += uJointMatrices[j2] * aWeights.z;
        skin += uJointMatrices[j3] * aWeights.w;
        position = skin * position;
    }

    vec4 worldPos = uModel * position;
    gl_Position = uMVP * position;
    vUV = aUV;
    vShadowCoord = uLightSpaceMatrix * worldPos;
    vWorldPos = worldPos.xyz;
}
