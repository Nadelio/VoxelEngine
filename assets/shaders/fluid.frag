#version 330 core
in vec2 vUV;
in vec2 vTileOrigin;
in vec4 vShadowCoord;
in vec3 vWorldPos;
in vec3 vNormal;
out vec4 FragColor;

uniform sampler2D uAtlas;
uniform sampler2D uShadowMap;
uniform mat4 uLightSpaceMatrix;
uniform vec2 uTileSize;
uniform vec3 uSunDirection;
uniform vec3 uSunColor;
uniform vec3 uAmbientColor;
uniform vec3 uPointLightPos;
uniform vec3 uPointLightColor;
uniform float uPointLightRange;
uniform float uPointLightIntensity;
uniform float uFluidAlpha;

vec3 QuantizeInsideFace(vec3 position, vec3 normal, float texelsPerUnit) {
    vec3 scaled = position * texelsPerUnit;
    vec3 q = floor(scaled);
    if(normal.x < -0.5) q.x = ceil(scaled.x);
    if(normal.y < -0.5) q.y = ceil(scaled.y);
    if(normal.z < -0.5) q.z = ceil(scaled.z);
    return q / texelsPerUnit;
}

float CompareShadow(vec3 projCoords, vec2 uv, float bias) {
    float depth = texture(uShadowMap, uv).r;
    return (projCoords.z - bias > depth) ? 1.0 : 0.0;
}

float SampleShadow(vec3 worldPos, vec3 normal) {
    const float TEXELS_PER_UNIT = 32.0;
    vec2 shadowTexSize = vec2(textureSize(uShadowMap, 0));

    vec3 sunDir = normalize(uSunDirection);
    float nDotL = max(dot(normal, sunDir), 0.0);
    float faceScale = mix(0.62, 1.0, abs(normal.y));
    float bias = max(0.00010, faceScale * 0.00040 * (1.0 - nDotL));

    vec3 samplePosA = worldPos - normal * (0.20 / TEXELS_PER_UNIT);
    vec3 qPosA = QuantizeInsideFace(samplePosA, normal, TEXELS_PER_UNIT);
    vec4 shadowCoordA = uLightSpaceMatrix * vec4(qPosA, 1.0);
    vec3 projA = shadowCoordA.xyz / max(shadowCoordA.w, 0.0001);
    projA = projA * 0.5 + 0.5;

    if(projA.z > 1.0 || projA.x < 0.0 || projA.x > 1.0 || projA.y < 0.0 || projA.y > 1.0)
        return 0.0;

    vec2 snappedUVA = (floor(projA.xy * shadowTexSize) + 0.5) / shadowTexSize;
    float shadowA = CompareShadow(projA, snappedUVA, bias);

    if(abs(normal.y) < 0.5) {
        vec3 samplePosB = worldPos - normal * (0.70 / TEXELS_PER_UNIT);
        vec3 qPosB = QuantizeInsideFace(samplePosB, normal, TEXELS_PER_UNIT);
        vec4 shadowCoordB = uLightSpaceMatrix * vec4(qPosB, 1.0);
        vec3 projB = shadowCoordB.xyz / max(shadowCoordB.w, 0.0001);
        projB = projB * 0.5 + 0.5;
        if(projB.z <= 1.0 && projB.x >= 0.0 && projB.x <= 1.0 && projB.y >= 0.0 && projB.y <= 1.0) {
            vec2 snappedUVB = (floor(projB.xy * shadowTexSize) + 0.5) / shadowTexSize;
            float shadowB = CompareShadow(projB, snappedUVB, bias);
            vec2 texel = 1.0 / shadowTexSize;
            shadowA = max(shadowA, CompareShadow(projA, snappedUVA + vec2(texel.x, 0.0), bias));
            shadowA = max(shadowA, CompareShadow(projA, snappedUVA - vec2(texel.x, 0.0), bias));
            shadowA = max(shadowA, CompareShadow(projA, snappedUVA + vec2(0.0, texel.y), bias));
            shadowA = max(shadowA, CompareShadow(projA, snappedUVA - vec2(0.0, texel.y), bias));
            shadowB = max(shadowB, CompareShadow(projB, snappedUVB + vec2(texel.x, 0.0), bias));
            shadowB = max(shadowB, CompareShadow(projB, snappedUVB - vec2(texel.x, 0.0), bias));
            shadowB = max(shadowB, CompareShadow(projB, snappedUVB + vec2(0.0, texel.y), bias));
            shadowB = max(shadowB, CompareShadow(projB, snappedUVB - vec2(0.0, texel.y), bias));
            return max(shadowA, shadowB);
        }
    }
    return shadowA;
}

void main() {
    vec2 tileUV = mod(vUV - vTileOrigin, uTileSize) + vTileOrigin;
    vec4 color = texture(uAtlas, tileUV);
    if(color.a < 0.05) discard;

    vec3 normal = normalize(vNormal);
    vec3 sunDir = normalize(uSunDirection);
    float sunNdotL = max(dot(normal, sunDir), 0.0);
    float shadow = SampleShadow(vWorldPos, normal);
    float sunVisibility = mix(1.0, 0.0, shadow);

    vec3 toPoint = uPointLightPos - vWorldPos;
    float pointDist = length(toPoint);
    vec3 pointDir = (pointDist > 0.0001) ? (toPoint / pointDist) : vec3(0.0, 1.0, 0.0);
    float pointNdotL = max(dot(normal, pointDir), 0.0);
    float pointFalloff = 1.0 - smoothstep(0.0, max(uPointLightRange, 0.001), pointDist);
    vec3 pointLight = uPointLightColor * (pointNdotL * pointFalloff * uPointLightIntensity);

    vec3 lit = uAmbientColor + uSunColor * (sunNdotL * sunVisibility) + pointLight;
    color.rgb *= clamp(lit, 0.0, 2.0);
    color.a = clamp(color.a * uFluidAlpha, 0.0, 1.0);
    FragColor = color;
}
