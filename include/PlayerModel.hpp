#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_opengl.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "AnimationHandler.hpp"
#include "CapeModel.hpp"
#include "Physics.hpp"
#include "Shader.hpp"
#include "SkinTexture.hpp"

class PlayerModel {
public:
    struct AnimationConfig {
        std::string rootBone     = "Waist";
        std::string headRootBone = "Head";

        std::unordered_map<std::string, std::string> thirdPersonAnims;
        std::unordered_map<std::string, std::string> firstPersonAnims;

        glm::vec3 thirdPersonCameraAnchor{0.0f, 0.62f, 0.0f};
        glm::vec3 firstPersonCameraAnchor{0.0f, 0.0f, 0.0f};

        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> thirdPersonTransitions;
        std::unordered_map<std::string, std::unordered_map<std::string, std::string>> firstPersonTransitions;
    };
    PlayerModel() = default;
    ~PlayerModel();

    PlayerModel(const PlayerModel&) = delete;
    PlayerModel& operator=(const PlayerModel&) = delete;

    bool Initialize();
    bool LoadSkin(const std::string& skinPath);
    bool LoadCape(const std::string& capePath);
    bool HasCape() const;
    void SetCapeEnabled(bool enabled);

    void UpdateAnimation(const Physics::Entity& player, float dtSeconds, bool sprinting);
    void Draw(
        Shader& skinShader,
        const glm::mat4& projection,
        const glm::mat4& view,
        const glm::mat4& lightSpaceMatrix,
        const Physics::Entity& player,
        const glm::vec3& cameraForward,
        bool firstPerson
    ) const;

    void DrawShadow(
        Shader& shadowShader,
        const glm::mat4& lightSpaceMatrix,
        const Physics::Entity& player,
        const glm::vec3& cameraForward,
        bool firstPerson
    ) const;

private:
    struct PrimitiveGpu {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
        bool hasSkinning = false;
    };

    struct Mesh {
        std::vector<PrimitiveGpu> primitives;
    };

    struct Node {
        std::string name;
        int parent = -1;
        std::vector<int> children;
        int mesh = -1;
        int skin = -1;
        glm::vec3 translation{0.0f};
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
    };

    struct Skin {
        std::vector<int> joints;
        std::vector<glm::mat4> inverseBindMatrices;
    };

    bool LoadGltf(const std::string& gltfPath);
    bool LoadAnimations(const std::string& animationDirectory);
    bool LoadModelConfig(const std::string& configPath);

    void ClearGpuMeshes();
    void DrawNodeRecursive(
        int nodeIndex,
        const glm::mat4& parent,
        const glm::mat4& root,
        const glm::mat4& projection,
        const glm::mat4& view,
        Shader& skinShader
    ) const;

    glm::mat4 ComputeRootTransform(
        const Physics::Entity& player,
        const glm::vec3& cameraForward
    ) const;

    void BuildAnimatedNodeTransforms(
        std::vector<glm::mat4>& localTransforms,
        std::vector<glm::mat4>& globalTransforms
    ) const;

    std::vector<Node> nodes_;
    std::vector<std::string> nodeAnimationKeys_;
    std::vector<int> rootNodes_;
    std::vector<Mesh> meshes_;
    std::vector<Skin> skins_;

    int capeAnchorNode_ = -1;

    AnimationConfig config_;
    SkinTexture skinTexture_;
    CapeModel capeModel_;
    AnimationHandler animationHandler_;
    const AnimationHandler::Clip* activeClip_ = nullptr;
    const AnimationHandler::Clip* previousClip_ = nullptr;
    std::string activeClipName_;
    float activeClipTime_ = 0.0f;
    float previousClipTime_ = 0.0f;
    float blendElapsedSeconds_ = 0.0f;
    float blendDurationSeconds_ = 0.14f;

    bool initialized_ = false;
    float renderScale_ = 0.09375f;
    bool capeEnabled_ = false;
    float capeFlap_ = 0.0f;
    float capeLean_ = 0.0f;
    float capeLean2_ = 0.0f;

    mutable float bodyYaw_ = 0.0f;
    mutable float lastCameraYaw_ = 0.0f;
    mutable float headYawOffset_ = 0.0f;
    mutable float headPitch_ = 0.0f;
};
