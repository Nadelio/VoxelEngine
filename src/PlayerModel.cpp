#include "PlayerModel.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

#include <SDL3/SDL.h>

#include <glm/common.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

namespace {
    PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
    PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
    PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
    PFNGLBUFFERDATAPROC pglBufferData = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
    PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;

    bool gLoadedGL = false;

    bool LoadGLFunctions() {
        if(gLoadedGL) {
            return true;
        }

        pglGenVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(SDL_GL_GetProcAddress("glGenVertexArrays"));
        pglBindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(SDL_GL_GetProcAddress("glBindVertexArray"));
        pglGenBuffers = reinterpret_cast<PFNGLGENBUFFERSPROC>(SDL_GL_GetProcAddress("glGenBuffers"));
        pglBindBuffer = reinterpret_cast<PFNGLBINDBUFFERPROC>(SDL_GL_GetProcAddress("glBindBuffer"));
        pglBufferData = reinterpret_cast<PFNGLBUFFERDATAPROC>(SDL_GL_GetProcAddress("glBufferData"));
        pglVertexAttribPointer = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(SDL_GL_GetProcAddress("glVertexAttribPointer"));
        pglEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
        pglDeleteVertexArrays = reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(SDL_GL_GetProcAddress("glDeleteVertexArrays"));
        pglDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(SDL_GL_GetProcAddress("glDeleteBuffers"));

        gLoadedGL = pglGenVertexArrays && pglBindVertexArray && pglGenBuffers && pglBindBuffer &&
                    pglBufferData && pglVertexAttribPointer && pglEnableVertexAttribArray &&
                    pglDeleteVertexArrays && pglDeleteBuffers;
        return gLoadedGL;
    }

    struct GltfBufferView {
        int buffer = -1;
        std::size_t byteOffset = 0;
        std::size_t byteLength = 0;
        std::size_t byteStride = 0;
    };

    struct GltfAccessor {
        int bufferView = -1;
        int componentType = 0;
        std::size_t count = 0;
        int type = TINYGLTF_TYPE_SCALAR;
        std::size_t byteOffset = 0;
    };

    struct CpuPrimitive {
        std::vector<float> vertices;
        std::vector<std::uint32_t> indices;
        bool hasSkinning = false;
    };

    struct GltfAnimationSampler {
        int inputAccessor = -1;
        int outputAccessor = -1;
        bool stepInterpolation = false;
    };

    struct SkinVertexData {
        std::vector<std::uint16_t> joints;
        std::vector<float> weights;
    };

    std::string ResolveAssetPath(std::filesystem::path relativePath) {
        if(std::filesystem::is_regular_file(relativePath)) {
            return relativePath.string();
        }

        if(const char* basePathRaw = SDL_GetBasePath()) {
            const std::filesystem::path basePath(basePathRaw);
            const std::array<std::filesystem::path, 3> prefixes = {
                "",
                "assets",
                "../assets",
            };

            for(const std::filesystem::path& prefix : prefixes) {
                const std::filesystem::path candidate = basePath / prefix / relativePath;
                if(std::filesystem::is_regular_file(candidate)) {
                    return candidate.string();
                }
            }
        }

        return relativePath.string();
    }

    int NumComponentsForType(int type) {
        switch(type) {
            case TINYGLTF_TYPE_SCALAR: return 1;
            case TINYGLTF_TYPE_VEC2: return 2;
            case TINYGLTF_TYPE_VEC3: return 3;
            case TINYGLTF_TYPE_VEC4: return 4;
            case TINYGLTF_TYPE_MAT4: return 16;
            default: return 0;
        }
    }

    std::size_t ComponentSizeBytes(int componentType) {
        switch(componentType) {
            case 5120: return 1;
            case 5121: return 1;
            case 5122: return 2;
            case 5123: return 2;
            case 5125: return 4;
            case 5126: return 4;
            default: return 0;
        }
    }

    bool ReadAccessorFloats(
        const GltfAccessor& accessor,
        const std::vector<GltfBufferView>& bufferViews,
        const std::vector<std::vector<std::uint8_t>>& buffers,
        std::vector<float>& out,
        int expectedComponents
    ) {
        if(accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(bufferViews.size())) {
            return false;
        }
        const GltfBufferView& view = bufferViews[static_cast<std::size_t>(accessor.bufferView)];
        if(view.buffer < 0 || view.buffer >= static_cast<int>(buffers.size())) {
            return false;
        }
        const std::vector<std::uint8_t>& buffer = buffers[static_cast<std::size_t>(view.buffer)];

        const int numComponents = NumComponentsForType(accessor.type);
        if(numComponents != expectedComponents) {
            return false;
        }
        if(accessor.componentType != 5126) {
            return false;
        }

        const std::size_t componentBytes = ComponentSizeBytes(accessor.componentType);
        const std::size_t packedStride = componentBytes * static_cast<std::size_t>(numComponents);
        const std::size_t stride = (view.byteStride > 0) ? view.byteStride : packedStride;
        const std::size_t start = view.byteOffset + accessor.byteOffset;

        if(start + stride * accessor.count > buffer.size()) {
            return false;
        }

        out.resize(accessor.count * static_cast<std::size_t>(numComponents));
        for(std::size_t i = 0; i < accessor.count; ++i) {
            const std::size_t srcBase = start + i * stride;
            for(int c = 0; c < numComponents; ++c) {
                float v = 0.0f;
                std::memcpy(&v, buffer.data() + srcBase + static_cast<std::size_t>(c) * componentBytes, sizeof(float));
                out[i * static_cast<std::size_t>(numComponents) + static_cast<std::size_t>(c)] = v;
            }
        }
        return true;
    }

    bool ReadAccessorIndices(
        const GltfAccessor& accessor,
        const std::vector<GltfBufferView>& bufferViews,
        const std::vector<std::vector<std::uint8_t>>& buffers,
        std::vector<std::uint32_t>& out
    ) {
        if(accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(bufferViews.size())) {
            return false;
        }
        if(accessor.type != TINYGLTF_TYPE_SCALAR) {
            return false;
        }

        const GltfBufferView& view = bufferViews[static_cast<std::size_t>(accessor.bufferView)];
        if(view.buffer < 0 || view.buffer >= static_cast<int>(buffers.size())) {
            return false;
        }
        const std::vector<std::uint8_t>& buffer = buffers[static_cast<std::size_t>(view.buffer)];

        const std::size_t componentBytes = ComponentSizeBytes(accessor.componentType);
        if(componentBytes == 0) {
            return false;
        }

        const std::size_t stride = (view.byteStride > 0) ? view.byteStride : componentBytes;
        const std::size_t start = view.byteOffset + accessor.byteOffset;
        if(start + stride * accessor.count > buffer.size()) {
            return false;
        }

        out.resize(accessor.count);
        for(std::size_t i = 0; i < accessor.count; ++i) {
            const std::size_t src = start + i * stride;
            std::uint32_t value = 0;
            if(accessor.componentType == 5121) {
                value = buffer[src];
            } else if(accessor.componentType == 5123) {
                std::uint16_t v = 0;
                std::memcpy(&v, buffer.data() + src, sizeof(std::uint16_t));
                value = v;
            } else if(accessor.componentType == 5125) {
                std::memcpy(&value, buffer.data() + src, sizeof(std::uint32_t));
            } else {
                return false;
            }
            out[i] = value;
        }
        return true;
    }

    bool ReadAccessorU16Vec4(
        const GltfAccessor& accessor,
        const std::vector<GltfBufferView>& bufferViews,
        const std::vector<std::vector<std::uint8_t>>& buffers,
        std::vector<std::uint16_t>& out
    ) {
        if(accessor.bufferView < 0 || accessor.bufferView >= static_cast<int>(bufferViews.size())) {
            return false;
        }
        if(accessor.componentType != 5121 && accessor.componentType != 5123) {
            return false;
        }
        if(accessor.type != TINYGLTF_TYPE_VEC4) {
            return false;
        }

        const GltfBufferView& view = bufferViews[static_cast<std::size_t>(accessor.bufferView)];
        if(view.buffer < 0 || view.buffer >= static_cast<int>(buffers.size())) {
            return false;
        }
        const std::vector<std::uint8_t>& buffer = buffers[static_cast<std::size_t>(view.buffer)];

        const std::size_t componentBytes = ComponentSizeBytes(accessor.componentType);
        const std::size_t packedStride = componentBytes * 4;
        const std::size_t stride = (view.byteStride > 0) ? view.byteStride : packedStride;
        const std::size_t start = view.byteOffset + accessor.byteOffset;
        if(start + stride * accessor.count > buffer.size()) {
            return false;
        }

        out.resize(accessor.count * 4);
        for(std::size_t i = 0; i < accessor.count; ++i) {
            const std::size_t srcBase = start + i * stride;
            for(int c = 0; c < 4; ++c) {
                std::uint16_t v = 0;
                if(accessor.componentType == 5121) {
                    v = static_cast<std::uint16_t>(buffer[srcBase + static_cast<std::size_t>(c)]);
                } else {
                    std::memcpy(&v, buffer.data() + srcBase + static_cast<std::size_t>(c) * componentBytes, sizeof(std::uint16_t));
                }
                out[i * 4 + static_cast<std::size_t>(c)] = v;
            }
        }
        return true;
    }

    glm::mat4 ComposeTRS(const glm::vec3& t, const glm::quat& r, const glm::vec3& s) {
        glm::mat4 m(1.0f);
        m = glm::translate(m, t);
        m *= glm::mat4_cast(r);
        m = glm::scale(m, s);
        return m;
    }

    glm::vec3 QuaternionToEulerDegrees(const glm::quat& qIn) {
        return glm::degrees(glm::eulerAngles(glm::normalize(qIn)));
    }

    bool NearlyEqualVec3(const glm::vec3& a, const glm::vec3& b, float epsilon = 0.001f) {
        return glm::all(glm::lessThanEqual(glm::abs(a - b), glm::vec3(epsilon)));
    }

}

PlayerModel::~PlayerModel() {
    ClearGpuMeshes();
}

void PlayerModel::ClearGpuMeshes() {
    for(Mesh& mesh : meshes_) {
        for(PrimitiveGpu& primitive : mesh.primitives) {
            if(primitive.ebo != 0 && pglDeleteBuffers) {
                pglDeleteBuffers(1, &primitive.ebo);
                primitive.ebo = 0;
            }
            if(primitive.vbo != 0 && pglDeleteBuffers) {
                pglDeleteBuffers(1, &primitive.vbo);
                primitive.vbo = 0;
            }
            if(primitive.vao != 0 && pglDeleteVertexArrays) {
                pglDeleteVertexArrays(1, &primitive.vao);
                primitive.vao = 0;
            }
            primitive.indexCount = 0;
        }
    }
}

bool PlayerModel::Initialize() {
    if(!LoadGLFunctions()) {
        SDL_Log("PlayerModel: failed to resolve required OpenGL functions.");
        return false;
    }

    ClearGpuMeshes();
    nodes_.clear();
    nodeAnimationKeys_.clear();
    rootNodes_.clear();
    meshes_.clear();
    skins_.clear();
    animationHandler_.Clear();

    const std::string modelPath = ResolveAssetPath("assets/models/player_model.gltf");
    if(!LoadGltf(modelPath)) {
        SDL_Log("PlayerModel: failed to load glTF model at %s", modelPath.c_str());
        return false;
    }

    if(animationHandler_.ClipNames().empty()) {
        const std::string animationsDir = ResolveAssetPath("assets/animations/player");
        LoadAnimations(animationsDir);
    }
    activeClip_ = animationHandler_.FindClip("idle");
    if(!activeClip_) {
        activeClip_ = animationHandler_.FindClip("walk");
    }
    if(activeClip_) {
        activeClipName_ = activeClip_->name;
    } else {
        activeClipName_.clear();
    }

    initialized_ = true;
    activeClipTime_ = 0.0f;
    previousClip_ = nullptr;
    previousClipTime_ = 0.0f;
    blendElapsedSeconds_ = blendDurationSeconds_;
    return true;
}

bool PlayerModel::LoadSkin(const std::string& skinPath) {
    return skinTexture_.LoadFromFile(skinPath);
}

bool PlayerModel::LoadCape(const std::string& capePath) {
    if(!capeModel_.IsLoaded()) {
        if(!capeModel_.Initialize()) {
            SDL_Log("PlayerModel::LoadCape failed to initialize cape model.");
            return false;
        }
    }
    return capeModel_.LoadCape(capePath);
}

bool PlayerModel::HasCape() const {
    return capeModel_.IsLoaded();
}

void PlayerModel::SetCapeEnabled(bool enabled) {
    capeEnabled_ = enabled;
}

bool PlayerModel::LoadAnimations(const std::string& animationDirectory) {
    return animationHandler_.LoadClipsFromDirectory(animationDirectory);
}

glm::mat4 PlayerModel::ComputeRootTransform(
    const Physics::Entity& player,
    const glm::vec3& cameraForward
) const {
    glm::vec3 horizontalForward(cameraForward.x, 0.0f, cameraForward.z);
    if(glm::length(horizontalForward) < 0.0001f) {
        horizontalForward = glm::vec3(0.0f, 0.0f, 1.0f);
    } else {
        horizontalForward = glm::normalize(horizontalForward);
    }

    const float yawRadians = std::atan2(horizontalForward.x, horizontalForward.z) + glm::pi<float>();
    const glm::vec3 feetPos = player.position - glm::vec3(0.0f, player.eyeFromFeet, 0.0f);

    glm::mat4 root(1.0f);
    root = glm::translate(root, feetPos + glm::vec3(0.0f, -0.03f, 0.0f));
    root = glm::rotate(root, yawRadians, glm::vec3(0.0f, 1.0f, 0.0f));
    root = glm::scale(root, glm::vec3(renderScale_));
    return root;
}

void PlayerModel::BuildAnimatedNodeTransforms(
    std::vector<glm::mat4>& localTransforms,
    std::vector<glm::mat4>& globalTransforms
) const {
    localTransforms.assign(nodes_.size(), glm::mat4(1.0f));
    globalTransforms.assign(nodes_.size(), glm::mat4(1.0f));

    for(std::size_t i = 0; i < nodes_.size(); ++i) {
        const Node& node = nodes_[i];

        glm::vec3 t = node.translation;
        glm::quat r = node.rotation;
        glm::vec3 s = node.scale;

        if(activeClip_ || previousClip_) {
            AnimationHandler::SampledBonePose currentPose;
            AnimationHandler::SampledBonePose previousPose;

            const std::string& nodeKey =
                (i < nodeAnimationKeys_.size() && !nodeAnimationKeys_[i].empty())
                    ? nodeAnimationKeys_[i]
                    : node.name;

            const bool hasCurrent = activeClip_ && (
                animationHandler_.SampleBone(*activeClip_, nodeKey, activeClipTime_, currentPose) ||
                animationHandler_.SampleBone(*activeClip_, node.name, activeClipTime_, currentPose)
            );
            const bool hasPrevious = previousClip_ && (
                animationHandler_.SampleBone(*previousClip_, nodeKey, previousClipTime_, previousPose) ||
                animationHandler_.SampleBone(*previousClip_, node.name, previousClipTime_, previousPose)
            );

            const float safeDuration = std::max(0.0001f, blendDurationSeconds_);
            const float blendAlpha = previousClip_
                ? glm::clamp(blendElapsedSeconds_ / safeDuration, 0.0f, 1.0f)
                : 1.0f;

            const glm::vec3 prevT = (hasPrevious && previousPose.hasTranslation) ? previousPose.translation : node.translation;
            const glm::quat prevR = (hasPrevious && previousPose.hasRotation)
                ? glm::normalize(previousPose.rotation)
                : glm::normalize(node.rotation);
            const glm::vec3 prevS = (hasPrevious && previousPose.hasScale) ? previousPose.scale : node.scale;

            const glm::vec3 curT = (hasCurrent && currentPose.hasTranslation) ? currentPose.translation : node.translation;
            const glm::quat curR = (hasCurrent && currentPose.hasRotation)
                ? glm::normalize(currentPose.rotation)
                : glm::normalize(node.rotation);
            const glm::vec3 curS = (hasCurrent && currentPose.hasScale) ? currentPose.scale : node.scale;

            t = glm::mix(prevT, curT, blendAlpha);
            r = glm::normalize(glm::slerp(prevR, curR, blendAlpha));
            s = glm::mix(prevS, curS, blendAlpha);
        }

        localTransforms[i] = ComposeTRS(t, r, s);
    }

    std::vector<char> globalComputed(nodes_.size(), 0);
    const std::function<glm::mat4(std::size_t)> computeGlobal = [&](std::size_t idx) -> glm::mat4 {
        if(globalComputed[idx]) {
            return globalTransforms[idx];
        }
        const Node& node = nodes_[idx];
        if(node.parent >= 0) {
            globalTransforms[idx] = computeGlobal(static_cast<std::size_t>(node.parent)) * localTransforms[idx];
        } else {
            globalTransforms[idx] = localTransforms[idx];
        }
        globalComputed[idx] = 1;
        return globalTransforms[idx];
    };

    for(std::size_t i = 0; i < nodes_.size(); ++i) {
        (void)computeGlobal(i);
    }
}

void PlayerModel::UpdateAnimation(const Physics::Entity& player, float dtSeconds, bool sprinting) {
    (void)player;

    const float speedXZ = std::sqrt(player.velocity.x * player.velocity.x + player.velocity.z * player.velocity.z);
    const bool moving = speedXZ > 0.05f;
    std::string wantedClip = "idle";
    if(player.posture == Physics::PostureState::CRAWLING) {
        wantedClip = moving ? "crawl" : "prone";
    } else if(player.posture == Physics::PostureState::CROUCHING && moving) {
        wantedClip = "crouch_walk";
    } else if(player.posture == Physics::PostureState::CROUCHING) {
        wantedClip = "crouch";
    } else if(!player.onGround) {
        wantedClip = "jump";
    } else if(moving && sprinting) {
        wantedClip = "sprint";
    } else if(moving) {
        wantedClip = "walk";
    }

    const AnimationHandler::Clip* nextClip = animationHandler_.FindClip(wantedClip);
    if(!nextClip) {
        if(moving) {
            nextClip = animationHandler_.FindClip("walk");
        }
        if(!nextClip) {
            nextClip = animationHandler_.FindClip("idle");
        }
    }

    if(nextClip != activeClip_) {
        previousClip_ = activeClip_;
        previousClipTime_ = activeClipTime_;

        activeClip_ = nextClip;
        activeClipTime_ = 0.0f;
        blendElapsedSeconds_ = 0.0f;
        activeClipName_ = activeClip_ ? activeClip_->name : std::string();
    }

    const float dt = std::max(0.0f, dtSeconds);
    activeClipTime_ += dt;

    if(previousClip_) {
        previousClipTime_ += dt;
        blendElapsedSeconds_ += dt;

        const float safeDuration = std::max(0.0001f, blendDurationSeconds_);
        if(blendElapsedSeconds_ >= safeDuration || !activeClip_) {
            previousClip_ = nullptr;
            previousClipTime_ = 0.0f;
            blendElapsedSeconds_ = safeDuration;
        }
    }

    const float speedXZ_cape = std::sqrt(player.velocity.x * player.velocity.x + player.velocity.z * player.velocity.z);
    const float flapAmount = glm::clamp(speedXZ_cape * 0.55f, 0.0f, 1.0f);
    capeFlap_ = std::sin(activeClipTime_ * 6.0f) * flapAmount * 1.8f;
    capeLean_ = speedXZ_cape;
    capeLean2_ = player.onGround ? 0.0f : glm::clamp(-player.velocity.y * 2.0f, -4.0f, 6.0f);
}

void PlayerModel::Draw(
    Shader& skinShader,
    const glm::mat4& projection,
    const glm::mat4& view,
    const glm::mat4& lightSpaceMatrix,
    const Physics::Entity& player,
    const glm::vec3& cameraForward
) const {
    static bool loggedMissingTexture = false;
    static bool loggedMissingShader = false;
    static bool loggedDrawStats = false;
    if(!initialized_) {
        return;
    }
    if(skinTexture_.TextureID() == 0) {
        if(!loggedMissingTexture) {
            std::fprintf(stderr, "PlayerModel::Draw skipped: skin texture not loaded (texture id is 0).\n");
            loggedMissingTexture = true;
        }
        return;
    }
    if(skinShader.Program() == 0) {
        if(!loggedMissingShader) {
            std::fprintf(stderr, "PlayerModel::Draw skipped: skin shader program is 0.\n");
            loggedMissingShader = true;
        }
        return;
    }

    const glm::mat4 root = ComputeRootTransform(player, cameraForward);
    const glm::vec3 feetPos = player.position - glm::vec3(0.0f, player.eyeFromFeet, 0.0f);
    const float yawRadians = std::atan2(cameraForward.x, cameraForward.z) + glm::pi<float>();

    const bool cullWasEnabled = glIsEnabled(GL_CULL_FACE) == GL_TRUE;
    glDisable(GL_CULL_FACE);

    skinShader.Use();
    skinTexture_.Bind(GL_TEXTURE0);
    skinShader.SetInt("uSkin", 0);
    skinShader.SetInt("uShadowMap", 1);
    skinShader.SetInt("uReceiveShadows", 1);
    skinShader.SetMat4("uLightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));
    skinShader.SetVec3("uSunDirection", 0.45f, 1.0f, 0.30f);
    skinShader.SetVec3("uSunColor", 0.95f, 0.93f, 0.90f);
    skinShader.SetVec3("uAmbientColor", 0.32f, 0.35f, 0.40f);
    const glm::vec3 pointPos = player.position + glm::vec3(0.0f, 0.9f, 0.0f);
    skinShader.SetVec3("uPointLightPos", pointPos.x, pointPos.y, pointPos.z);
    skinShader.SetVec3("uPointLightColor", 0.0f, 0.0f, 0.0f);
    skinShader.SetFloat("uPointLightRange", 1.0f);
    skinShader.SetFloat("uPointLightIntensity", 0.0f);

    std::vector<glm::mat4> localTransforms;
    std::vector<glm::mat4> globalTransforms;
    BuildAnimatedNodeTransforms(localTransforms, globalTransforms);

    constexpr std::size_t kMaxSkinJoints = 64;
    std::size_t drawnPrimitiveCount = 0;
    std::size_t skinnedPrimitiveCount = 0;
    std::size_t drawnIndexCount = 0;
    for(std::size_t nodeIndex = 0; nodeIndex < nodes_.size(); ++nodeIndex) {
        const Node& node = nodes_[nodeIndex];
        if(node.mesh < 0 || node.mesh >= static_cast<int>(meshes_.size())) {
            continue;
        }

        const glm::mat4 model = root * globalTransforms[nodeIndex];
        const glm::mat4 mvp = projection * view * model;
        skinShader.SetMat4("uMVP", glm::value_ptr(mvp));
        skinShader.SetMat4("uModel", glm::value_ptr(model));

        std::vector<glm::mat4> jointMatrices;
        bool canSkin = false;
        if(node.skin >= 0 && node.skin < static_cast<int>(skins_.size())) {
            const Skin& skin = skins_[static_cast<std::size_t>(node.skin)];
            const std::size_t jointCount = std::min(kMaxSkinJoints, skin.joints.size());
            if(jointCount > 0) {
                jointMatrices.assign(jointCount, glm::mat4(1.0f));
                for(std::size_t j = 0; j < jointCount; ++j) {
                    const int jointNode = skin.joints[j];
                    if(jointNode < 0 || jointNode >= static_cast<int>(globalTransforms.size())) {
                        continue;
                    }
                    const glm::mat4 invBind = (j < skin.inverseBindMatrices.size())
                        ? skin.inverseBindMatrices[j]
                        : glm::mat4(1.0f);
                    const glm::mat4 invMeshGlobal = glm::inverse(globalTransforms[nodeIndex]);
                    jointMatrices[j] = invMeshGlobal * globalTransforms[static_cast<std::size_t>(jointNode)] * invBind;
                }
                canSkin = true;
            }
        }

        const Mesh& mesh = meshes_[static_cast<std::size_t>(node.mesh)];
        for(const PrimitiveGpu& primitive : mesh.primitives) {
            const bool useSkinning = canSkin && primitive.hasSkinning;
            skinShader.SetInt("uUseSkinning", useSkinning ? 1 : 0);
            if(useSkinning) {
                skinShader.SetInt("uJointCount", static_cast<int>(jointMatrices.size()));
                skinShader.SetMat4Array("uJointMatrices", glm::value_ptr(jointMatrices[0]), jointMatrices.size());
            } else {
                skinShader.SetInt("uJointCount", 0);
            }

            if(primitive.vao == 0 || primitive.indexCount <= 0) {
                continue;
            }
            pglBindVertexArray(primitive.vao);
            glDrawElements(GL_TRIANGLES, primitive.indexCount, GL_UNSIGNED_INT, nullptr);
            ++drawnPrimitiveCount;
            drawnIndexCount += static_cast<std::size_t>(primitive.indexCount);
            if(useSkinning) {
                ++skinnedPrimitiveCount;
            }
        }
        pglBindVertexArray(0);
    }

    if(capeEnabled_ && capeModel_.IsLoaded()) {
        const glm::mat4 root = ComputeRootTransform(player, cameraForward);
        glm::vec3 forward(cameraForward.x, 0.0f, cameraForward.z);
        if(glm::length(forward) < 0.0001f) {
            forward = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            forward = glm::normalize(forward);
        }
        const glm::vec2 velXZ(player.velocity.x, player.velocity.z);
        const float forwardVel = glm::dot(glm::vec2(forward.x, forward.z), velXZ);
        const float capeMotionLean = glm::clamp(-forwardVel * 3.0f, -10.0f, 16.0f);
        capeModel_.Draw(
            skinShader,
            projection,
            view,
            root,
            capeFlap_,
            capeMotionLean,
            capeLean2_
        );
    }

    if(cullWasEnabled) {
        glEnable(GL_CULL_FACE);
    }

    if(!loggedDrawStats) {
        std::fprintf(
            stderr,
            "PlayerModel::Draw stats: nodes=%zu meshes=%zu skins=%zu primitives=%zu skinned=%zu indices=%zu feet=(%.2f, %.2f, %.2f) yaw=%.2f scale=%.5f\n",
            nodes_.size(),
            meshes_.size(),
            skins_.size(),
            drawnPrimitiveCount,
            skinnedPrimitiveCount,
            drawnIndexCount,
            feetPos.x,
            feetPos.y,
            feetPos.z,
            yawRadians,
            renderScale_
        );
        loggedDrawStats = true;
    }
}

void PlayerModel::DrawShadow(
    Shader& shadowShader,
    const glm::mat4& lightSpaceMatrix,
    const Physics::Entity& player,
    const glm::vec3& cameraForward
) const {
    if(!initialized_) {
        return;
    }

    const glm::mat4 root = ComputeRootTransform(player, cameraForward);
    std::vector<glm::mat4> localTransforms;
    std::vector<glm::mat4> globalTransforms;
    BuildAnimatedNodeTransforms(localTransforms, globalTransforms);

    shadowShader.Use();

    constexpr std::size_t kMaxSkinJoints = 64;
    for(std::size_t nodeIndex = 0; nodeIndex < nodes_.size(); ++nodeIndex) {
        const Node& node = nodes_[nodeIndex];
        if(node.mesh < 0 || node.mesh >= static_cast<int>(meshes_.size())) {
            continue;
        }

        const glm::mat4 model = root * globalTransforms[nodeIndex];
        const glm::mat4 lightMvp = lightSpaceMatrix * model;
        shadowShader.SetMat4("uLightMVP", glm::value_ptr(lightMvp));

        std::vector<glm::mat4> jointMatrices;
        bool canSkin = false;
        if(node.skin >= 0 && node.skin < static_cast<int>(skins_.size())) {
            const Skin& skin = skins_[static_cast<std::size_t>(node.skin)];
            const std::size_t jointCount = std::min(kMaxSkinJoints, skin.joints.size());
            if(jointCount > 0) {
                jointMatrices.assign(jointCount, glm::mat4(1.0f));
                for(std::size_t j = 0; j < jointCount; ++j) {
                    const int jointNode = skin.joints[j];
                    if(jointNode < 0 || jointNode >= static_cast<int>(globalTransforms.size())) {
                        continue;
                    }
                    const glm::mat4 invBind = (j < skin.inverseBindMatrices.size())
                        ? skin.inverseBindMatrices[j]
                        : glm::mat4(1.0f);
                    const glm::mat4 invMeshGlobal = glm::inverse(globalTransforms[nodeIndex]);
                    jointMatrices[j] = invMeshGlobal * globalTransforms[static_cast<std::size_t>(jointNode)] * invBind;
                }
                canSkin = true;
            }
        }

        const Mesh& mesh = meshes_[static_cast<std::size_t>(node.mesh)];
        for(const PrimitiveGpu& primitive : mesh.primitives) {
            const bool useSkinning = canSkin && primitive.hasSkinning;
            shadowShader.SetInt("uUseSkinning", useSkinning ? 1 : 0);
            if(useSkinning) {
                shadowShader.SetInt("uJointCount", static_cast<int>(jointMatrices.size()));
                shadowShader.SetMat4Array("uJointMatrices", glm::value_ptr(jointMatrices[0]), jointMatrices.size());
            } else {
                shadowShader.SetInt("uJointCount", 0);
            }
            if(primitive.vao == 0 || primitive.indexCount <= 0) {
                continue;
            }
            pglBindVertexArray(primitive.vao);
            glDrawElements(GL_TRIANGLES, primitive.indexCount, GL_UNSIGNED_INT, nullptr);
        }
        pglBindVertexArray(0);
    }

    if(capeEnabled_ && capeModel_.IsLoaded()) {
        glm::vec3 forward(cameraForward.x, 0.0f, cameraForward.z);
        if(glm::length(forward) < 0.0001f) {
            forward = glm::vec3(0.0f, 0.0f, 1.0f);
        } else {
            forward = glm::normalize(forward);
        }
        const glm::vec2 velXZ(player.velocity.x, player.velocity.z);
        const float forwardVel = glm::dot(glm::vec2(forward.x, forward.z), velXZ);
        const float capeMotionLean = glm::clamp(-forwardVel * 3.0f, -10.0f, 16.0f);
        capeModel_.DrawShadow(
            shadowShader,
            lightSpaceMatrix,
            root,
            capeFlap_,
            capeMotionLean,
            capeLean2_
        );
    }
}

void PlayerModel::DrawNodeRecursive(
    int nodeIndex,
    const glm::mat4& parent,
    const glm::mat4& root,
    const glm::mat4& projection,
    const glm::mat4& view,
    Shader& skinShader
) const {
    if(nodeIndex < 0 || nodeIndex >= static_cast<int>(nodes_.size())) {
        return;
    }
    const Node& node = nodes_[static_cast<std::size_t>(nodeIndex)];

    glm::vec3 t = node.translation;
    glm::quat r = node.rotation;
    glm::vec3 s = node.scale;

    if(activeClip_ || previousClip_) {
        AnimationHandler::SampledBonePose currentPose;
        AnimationHandler::SampledBonePose previousPose;

        const bool hasCurrent = activeClip_ && animationHandler_.SampleBone(*activeClip_, node.name, activeClipTime_, currentPose);
        const bool hasPrevious = previousClip_ && animationHandler_.SampleBone(*previousClip_, node.name, previousClipTime_, previousPose);

        const float safeDuration = std::max(0.0001f, blendDurationSeconds_);
        const float blendAlpha = previousClip_
            ? glm::clamp(blendElapsedSeconds_ / safeDuration, 0.0f, 1.0f)
            : 1.0f;

        const glm::vec3 prevT = (hasPrevious && previousPose.hasTranslation) ? previousPose.translation : glm::vec3(0.0f);
        const glm::quat prevR = (hasPrevious && previousPose.hasRotation)
            ? glm::normalize(previousPose.rotation)
            : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        const glm::vec3 prevS = (hasPrevious && previousPose.hasScale) ? previousPose.scale : glm::vec3(1.0f);

        const glm::vec3 curT = (hasCurrent && currentPose.hasTranslation) ? currentPose.translation : glm::vec3(0.0f);
        const glm::quat curR = (hasCurrent && currentPose.hasRotation)
            ? glm::normalize(currentPose.rotation)
            : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        const glm::vec3 curS = (hasCurrent && currentPose.hasScale) ? currentPose.scale : glm::vec3(1.0f);

        t += glm::mix(prevT, curT, blendAlpha);
        r = r * glm::normalize(glm::slerp(prevR, curR, blendAlpha));
        s *= glm::mix(prevS, curS, blendAlpha);
    }

    const glm::mat4 local = ComposeTRS(t, r, s);
    const glm::mat4 world = parent * local;

    if(node.mesh >= 0 && node.mesh < static_cast<int>(meshes_.size())) {
        const glm::mat4 mvp = projection * view * root * world;
        skinShader.SetMat4("uMVP", glm::value_ptr(mvp));

        const Mesh& mesh = meshes_[static_cast<std::size_t>(node.mesh)];
        for(const PrimitiveGpu& primitive : mesh.primitives) {
            if(primitive.vao == 0 || primitive.indexCount <= 0) {
                continue;
            }
            pglBindVertexArray(primitive.vao);
            glDrawElements(GL_TRIANGLES, primitive.indexCount, GL_UNSIGNED_INT, nullptr);
        }
        pglBindVertexArray(0);
    }

    for(const int child : node.children) {
        DrawNodeRecursive(child, world, root, projection, view, skinShader);
    }
}

bool PlayerModel::LoadGltf(const std::string& modelPath) {
    tinygltf::Model model;
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    loader.SetImageLoader(
        [](tinygltf::Image* image,
           const int,
           std::string*,
           std::string*,
           int,
           int,
           const unsigned char*,
           int,
           void*) -> bool {
            if(image) {
                image->width = 1;
                image->height = 1;
                image->component = 4;
                image->bits = 8;
                image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                image->image.assign({255, 255, 255, 255});
            }
            return true;
        },
        nullptr
    );

    bool success = loader.LoadASCIIFromFile(&model, &err, &warn, modelPath);
    if (!success) {
        SDL_Log("Failed to load glTF model: %s", err.c_str());
        return false;
    }

    if (!warn.empty()) {
        SDL_Log("glTF warning: %s", warn.c_str());
    }

    std::vector<GltfBufferView> bufferViews;
    bufferViews.reserve(model.bufferViews.size());
    for(const tinygltf::BufferView& bufferView : model.bufferViews) {
        GltfBufferView view;
        view.buffer = bufferView.buffer;
        view.byteOffset = static_cast<std::size_t>(bufferView.byteOffset);
        view.byteLength = static_cast<std::size_t>(bufferView.byteLength);
        view.byteStride = static_cast<std::size_t>(bufferView.byteStride);
        bufferViews.push_back(view);
    }

    std::vector<GltfAccessor> accessors;
    accessors.reserve(model.accessors.size());
    for(const tinygltf::Accessor& accessor : model.accessors) {
        GltfAccessor acc;
        acc.bufferView = accessor.bufferView;
        acc.componentType = accessor.componentType;
        acc.count = static_cast<std::size_t>(accessor.count);
        acc.type = accessor.type;
        acc.byteOffset = static_cast<std::size_t>(accessor.byteOffset);
        accessors.push_back(acc);
    }

    std::vector<std::vector<std::uint8_t>> buffers;
    buffers.reserve(model.buffers.size());
    for(const tinygltf::Buffer& buffer : model.buffers) {
        buffers.push_back(buffer.data);
    }

    nodes_.clear();
    nodeAnimationKeys_.clear();
    rootNodes_.clear();
    meshes_.clear();
    skins_.clear();

    nodes_.resize(model.nodes.size());
    nodeAnimationKeys_.resize(model.nodes.size());

    for(std::size_t i = 0; i < model.nodes.size(); ++i) {
        const tinygltf::Node& src = model.nodes[i];
        Node& dst = nodes_[i];

        dst.name = src.name.empty() ? ("node_" + std::to_string(i)) : src.name;
        nodeAnimationKeys_[i] = dst.name + "#" + std::to_string(i);
        dst.mesh = src.mesh;
        dst.skin = src.skin;

        if(src.translation.size() == 3) {
            dst.translation = glm::vec3(
                static_cast<float>(src.translation[0]),
                static_cast<float>(src.translation[1]),
                static_cast<float>(src.translation[2])
            );
        }

        if(src.rotation.size() == 4) {
            dst.rotation = glm::normalize(glm::quat(
                static_cast<float>(src.rotation[3]),
                static_cast<float>(src.rotation[0]),
                static_cast<float>(src.rotation[1]),
                static_cast<float>(src.rotation[2])
            ));
        }

        if(src.scale.size() == 3) {
            dst.scale = glm::vec3(
                static_cast<float>(src.scale[0]),
                static_cast<float>(src.scale[1]),
                static_cast<float>(src.scale[2])
            );
        }

        for(int child : src.children) {
            if(child >= 0 && child < static_cast<int>(nodes_.size())) {
                dst.children.push_back(child);
                nodes_[static_cast<std::size_t>(child)].parent = static_cast<int>(i);
            }
        }
    }

    int sceneIndex = model.defaultScene;
    if(sceneIndex < 0 && !model.scenes.empty()) {
        sceneIndex = 0;
    }
    if(sceneIndex >= 0 && sceneIndex < static_cast<int>(model.scenes.size())) {
        for(const int root : model.scenes[static_cast<std::size_t>(sceneIndex)].nodes) {
            if(root >= 0 && root < static_cast<int>(nodes_.size())) {
                rootNodes_.push_back(root);
            }
        }
    }
    if(rootNodes_.empty()) {
        for(std::size_t i = 0; i < nodes_.size(); ++i) {
            if(nodes_[i].parent < 0) {
                rootNodes_.push_back(static_cast<int>(i));
            }
        }
    }

    meshes_.resize(model.meshes.size());
    for(std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
        const tinygltf::Mesh& srcMesh = model.meshes[meshIndex];
        Mesh& dstMesh = meshes_[meshIndex];

        for(const tinygltf::Primitive& primitive : srcMesh.primitives) {
            auto posIt = primitive.attributes.find("POSITION");
            if(posIt == primitive.attributes.end()) {
                continue;
            }

            const int posAccessorIndex = posIt->second;
            if(posAccessorIndex < 0 || posAccessorIndex >= static_cast<int>(accessors.size())) {
                continue;
            }

            std::vector<float> positions;
            if(!ReadAccessorFloats(accessors[static_cast<std::size_t>(posAccessorIndex)], bufferViews, buffers, positions, 3)) {
                continue;
            }

            const std::size_t vertexCount = positions.size() / 3;
            std::vector<float> uvs(vertexCount * 2, 0.0f);
            std::vector<std::uint16_t> joints(vertexCount * 4, 0);
            std::vector<float> weights(vertexCount * 4, 0.0f);

            if(auto uvIt = primitive.attributes.find("TEXCOORD_0"); uvIt != primitive.attributes.end()) {
                const int uvAccessorIndex = uvIt->second;
                if(uvAccessorIndex >= 0 && uvAccessorIndex < static_cast<int>(accessors.size())) {
                    std::vector<float> loadedUVs;
                    if(ReadAccessorFloats(accessors[static_cast<std::size_t>(uvAccessorIndex)], bufferViews, buffers, loadedUVs, 2) && loadedUVs.size() == uvs.size()) {
                        uvs = std::move(loadedUVs);
                    }
                }
            }

            bool hasSkinning = false;
            if(auto jointsIt = primitive.attributes.find("JOINTS_0"); jointsIt != primitive.attributes.end()) {
                const int jointAccessorIndex = jointsIt->second;
                if(jointAccessorIndex >= 0 && jointAccessorIndex < static_cast<int>(accessors.size())) {
                    hasSkinning = ReadAccessorU16Vec4(
                        accessors[static_cast<std::size_t>(jointAccessorIndex)],
                        bufferViews,
                        buffers,
                        joints
                    );
                }
            }

            if(auto weightIt = primitive.attributes.find("WEIGHTS_0"); weightIt != primitive.attributes.end()) {
                const int weightAccessorIndex = weightIt->second;
                if(weightAccessorIndex >= 0 && weightAccessorIndex < static_cast<int>(accessors.size())) {
                    std::vector<float> loadedWeights;
                    if(ReadAccessorFloats(accessors[static_cast<std::size_t>(weightAccessorIndex)], bufferViews, buffers, loadedWeights, 4) && loadedWeights.size() == weights.size()) {
                        weights = std::move(loadedWeights);
                        hasSkinning = hasSkinning || std::any_of(weights.begin(), weights.end(), [](float w) { return w > 0.0f; });
                    }
                }
            }

            std::vector<std::uint32_t> indices;
            if(primitive.indices >= 0) {
                if(primitive.indices >= static_cast<int>(accessors.size())) {
                    continue;
                }
                if(!ReadAccessorIndices(accessors[static_cast<std::size_t>(primitive.indices)], bufferViews, buffers, indices)) {
                    continue;
                }
            } else {
                indices.resize(vertexCount);
                for(std::size_t i = 0; i < vertexCount; ++i) {
                    indices[i] = static_cast<std::uint32_t>(i);
                }
            }

            std::vector<float> interleaved;
            interleaved.resize(vertexCount * 13);
            for(std::size_t i = 0; i < vertexCount; ++i) {
                const std::size_t srcPos = i * 3;
                const std::size_t srcUv = i * 2;
                const std::size_t srcJoint = i * 4;
                const std::size_t dst = i * 13;

                interleaved[dst + 0] = positions[srcPos + 0];
                interleaved[dst + 1] = positions[srcPos + 1];
                interleaved[dst + 2] = positions[srcPos + 2];
                interleaved[dst + 3] = uvs[srcUv + 0];
                interleaved[dst + 4] = 1.0f - uvs[srcUv + 1];
                interleaved[dst + 5] = static_cast<float>(joints[srcJoint + 0]);
                interleaved[dst + 6] = static_cast<float>(joints[srcJoint + 1]);
                interleaved[dst + 7] = static_cast<float>(joints[srcJoint + 2]);
                interleaved[dst + 8] = static_cast<float>(joints[srcJoint + 3]);
                interleaved[dst + 9] = weights[srcJoint + 0];
                interleaved[dst + 10] = weights[srcJoint + 1];
                interleaved[dst + 11] = weights[srcJoint + 2];
                interleaved[dst + 12] = weights[srcJoint + 3];
            }

            PrimitiveGpu gpu;
            gpu.hasSkinning = hasSkinning;
            gpu.indexCount = static_cast<GLsizei>(indices.size());

            pglGenVertexArrays(1, &gpu.vao);
            pglGenBuffers(1, &gpu.vbo);
            pglGenBuffers(1, &gpu.ebo);

            pglBindVertexArray(gpu.vao);
            pglBindBuffer(GL_ARRAY_BUFFER, gpu.vbo);
            pglBufferData(
                GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(interleaved.size() * sizeof(float)),
                interleaved.data(),
                GL_STATIC_DRAW
            );

            pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.ebo);
            pglBufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                indices.data(),
                GL_STATIC_DRAW
            );

            const GLsizei stride = static_cast<GLsizei>(13 * sizeof(float));
            pglEnableVertexAttribArray(0);
            pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));

            pglEnableVertexAttribArray(1);
            pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));

            pglEnableVertexAttribArray(2);
            pglVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(5 * sizeof(float)));

            pglEnableVertexAttribArray(3);
            pglVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(9 * sizeof(float)));

            pglBindVertexArray(0);

            dstMesh.primitives.push_back(gpu);
        }
    }

    skins_.resize(model.skins.size());
    for(std::size_t skinIndex = 0; skinIndex < model.skins.size(); ++skinIndex) {
        const tinygltf::Skin& srcSkin = model.skins[skinIndex];
        Skin& dstSkin = skins_[skinIndex];

        dstSkin.joints = srcSkin.joints;

        if(srcSkin.inverseBindMatrices >= 0 && srcSkin.inverseBindMatrices < static_cast<int>(accessors.size())) {
            std::vector<float> ibmFloats;
            if(ReadAccessorFloats(accessors[static_cast<std::size_t>(srcSkin.inverseBindMatrices)], bufferViews, buffers, ibmFloats, 16)) {
                const std::size_t matrixCount = ibmFloats.size() / 16;
                dstSkin.inverseBindMatrices.resize(matrixCount, glm::mat4(1.0f));
                for(std::size_t i = 0; i < matrixCount; ++i) {
                    dstSkin.inverseBindMatrices[i] = glm::make_mat4(ibmFloats.data() + i * 16);
                }
            }
        }
    }

    for(std::size_t animationIndex = 0; animationIndex < model.animations.size(); ++animationIndex) {
        const tinygltf::Animation& srcAnim = model.animations[animationIndex];
        AnimationHandler::Clip clip;
        clip.name = srcAnim.name.empty() ? ("clip_" + std::to_string(animationIndex)) : srcAnim.name;
        clip.loop = true;

        for(const tinygltf::AnimationChannel& channel : srcAnim.channels) {
            if(channel.sampler < 0 || channel.sampler >= static_cast<int>(srcAnim.samplers.size())) {
                continue;
            }
            if(channel.target_node < 0 || channel.target_node >= static_cast<int>(nodes_.size())) {
                continue;
            }

            const tinygltf::AnimationSampler& sampler = srcAnim.samplers[static_cast<std::size_t>(channel.sampler)];
            if(sampler.input < 0 || sampler.input >= static_cast<int>(accessors.size()) ||
               sampler.output < 0 || sampler.output >= static_cast<int>(accessors.size())) {
                continue;
            }

            std::vector<float> times;
            if(!ReadAccessorFloats(accessors[static_cast<std::size_t>(sampler.input)], bufferViews, buffers, times, 1)) {
                continue;
            }

            const std::string& nodeKey =
                (static_cast<std::size_t>(channel.target_node) < nodeAnimationKeys_.size() &&
                 !nodeAnimationKeys_[static_cast<std::size_t>(channel.target_node)].empty())
                    ? nodeAnimationKeys_[static_cast<std::size_t>(channel.target_node)]
                    : nodes_[static_cast<std::size_t>(channel.target_node)].name;

            AnimationHandler::BoneChannel& bone = clip.bones[nodeKey];

            if(channel.target_path == "translation") {
                std::vector<float> values;
                if(!ReadAccessorFloats(accessors[static_cast<std::size_t>(sampler.output)], bufferViews, buffers, values, 3)) {
                    continue;
                }

                const Node& targetNode = nodes_[static_cast<std::size_t>(channel.target_node)];
                const bool rebaseFromParent = [&]() {
                    if(values.size() < 3 || targetNode.parent < 0 || targetNode.parent >= static_cast<int>(nodes_.size())) {
                        return false;
                    }

                    const Node& parentNode = nodes_[static_cast<std::size_t>(targetNode.parent)];
                    if(parentNode.mesh < 0 || !NearlyEqualVec3(targetNode.translation, glm::vec3(0.0f))) {
                        return false;
                    }

                    return true;
                }();
                const glm::vec3 rebaseOffset = rebaseFromParent
                    ? glm::vec3(0.0f, nodes_[static_cast<std::size_t>(targetNode.parent)].translation.y, 0.0f)
                    : glm::vec3(0.0f);

                const std::size_t count = std::min(times.size(), values.size() / 3);
                for(std::size_t i = 0; i < count; ++i) {
                    AnimationHandler::KeyframeVec3 keyframe;
                    keyframe.timeSeconds = times[i];
                    keyframe.stepInterpolation = (sampler.interpolation == "STEP");
                    keyframe.value = glm::vec3(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]) - rebaseOffset;
                    bone.translation.push_back(keyframe);
                    clip.lengthSeconds = std::max(clip.lengthSeconds, keyframe.timeSeconds);
                }
            } else if(channel.target_path == "scale") {
                std::vector<float> values;
                if(!ReadAccessorFloats(accessors[static_cast<std::size_t>(sampler.output)], bufferViews, buffers, values, 3)) {
                    continue;
                }
                const std::size_t count = std::min(times.size(), values.size() / 3);
                for(std::size_t i = 0; i < count; ++i) {
                    AnimationHandler::KeyframeVec3 keyframe;
                    keyframe.timeSeconds = times[i];
                    keyframe.stepInterpolation = (sampler.interpolation == "STEP");
                    keyframe.value = glm::vec3(values[i * 3 + 0], values[i * 3 + 1], values[i * 3 + 2]);
                    bone.scale.push_back(keyframe);
                    clip.lengthSeconds = std::max(clip.lengthSeconds, keyframe.timeSeconds);
                }
            } else if(channel.target_path == "rotation") {
                std::vector<float> values;
                if(!ReadAccessorFloats(accessors[static_cast<std::size_t>(sampler.output)], bufferViews, buffers, values, 4)) {
                    continue;
                }
                const std::size_t count = std::min(times.size(), values.size() / 4);
                for(std::size_t i = 0; i < count; ++i) {
                    const glm::quat q(
                        values[i * 4 + 3],
                        values[i * 4 + 0],
                        values[i * 4 + 1],
                        values[i * 4 + 2]
                    );

                    AnimationHandler::KeyframeVec3 keyframe;
                    keyframe.timeSeconds = times[i];
                    keyframe.stepInterpolation = (sampler.interpolation == "STEP");
                    keyframe.value = QuaternionToEulerDegrees(q);
                    bone.rotationDegrees.push_back(keyframe);
                    clip.lengthSeconds = std::max(clip.lengthSeconds, keyframe.timeSeconds);
                }
            }
        }

        if(!clip.bones.empty()) {
            animationHandler_.AddOrReplaceClip(std::move(clip));
        }
    }

    return true;
}
