#version 330 core
in vec2 vUV;
in vec4 vShadowCoord;
in vec3 vWorldPos;
out vec4 FragColor;

uniform sampler2D uSkin;
uniform sampler2D uShadowMap;
uniform mat4 uLightSpaceMatrix;
uniform int uReceiveShadows;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform vec3 uAmbientColor;
uniform vec3 uPointLightPos;
uniform vec3 uPointLightColor;
uniform float uPointLightRange;
uniform float uPointLightIntensity;

float SampleShadow(vec3 worldPos) {
    const float TEXELS_PER_UNIT = 32.0;
    vec3 qPos = floor(worldPos * TEXELS_PER_UNIT) / TEXELS_PER_UNIT;

    vec4 shadowCoord = uLightSpaceMatrix * vec4(qPos, 1.0);
    vec3 projCoords = shadowCoord.xyz / max(shadowCoord.w, 0.0001);
    projCoords = projCoords * 0.5 + 0.5;

    if(projCoords.z > 1.0 || projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0) {
        return 0.0;
    }

    const float bias = 0.00015;
    float closestDepth = texture(uShadowMap, projCoords.xy).r;
    return (projCoords.z - bias > closestDepth) ? 1.0 : 0.0;
}

void main() {
    vec4 color = texture(uSkin, vUV);
    if (color.a < 0.1) discard;

    vec3 normal = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));
    vec3 sunDir = normalize(uSunDirection);
    float sunNdotL = max(dot(normal, sunDir), 0.0);

    float shadow = 0.0;
    if(uReceiveShadows != 0) {
        shadow = SampleShadow(vWorldPos);
    }

    float sunVisibility = mix(1.0, 0.0, shadow);

    vec3 toPoint = uPointLightPos - vWorldPos;
    float pointDist = length(toPoint);
    vec3 pointDir = (pointDist > 0.0001) ? (toPoint / pointDist) : vec3(0.0, 1.0, 0.0);
    float pointNdotL = max(dot(normal, pointDir), 0.0);
    float pointFalloff = 1.0 - smoothstep(0.0, max(uPointLightRange, 0.001), pointDist);
    vec3 pointLight = uPointLightColor * (pointNdotL * pointFalloff * uPointLightIntensity);

    vec3 lit = uAmbientColor + uSunColor * (sunNdotL * sunVisibility) + pointLight;
    color.rgb *= clamp(lit, 0.0, 2.0);
    FragColor = color;
}
