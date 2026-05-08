#pragma once

#include <cstdint>
#include <string>

#include <SDL3/SDL_opengl.h>

#include "AtlasTexture.hpp"
#include "BlockRegistry.hpp"
#include "CubeMesh.hpp"
#include "Shader.hpp"
#include "SkinUV.hpp"
#include "SkinTexture.hpp"

class HandModel {
public:
    HandModel() = default;
    ~HandModel();

    HandModel(const HandModel&) = delete;
    HandModel& operator=(const HandModel&) = delete;

    bool Initialize();
    bool LoadSkin(const std::string& skinPath);

    void TriggerSwing();
    void TriggerPoint();
    void Update(float dtSeconds);

    void SetBlock(uint32_t blockID, const BlockRegistry& registry, const AtlasTexture& atlas);
    void ClearBlock();

    void Draw(
        Shader& blockShader,
        Shader& skinShader,
        const AtlasTexture& blockAtlas,
        int viewportWidth,
        int viewportHeight
    ) const;

private:
    bool initializeArmMeshes();

    static bool loadGLFunctions();
    static void destroyMesh(GLuint& vao, GLuint& vbo, GLuint& ebo);

    static bool createCubeMeshWithUV(const SkinUV::Cube& cubeUV, GLuint& outVao, GLuint& outVbo, GLuint& outEbo);

    SkinTexture skinTexture_;
    CubeMesh heldBlockMesh_;

    GLuint armVao_ = 0;
    GLuint armVbo_ = 0;
    GLuint armEbo_ = 0;

    GLuint sleeveVao_ = 0;
    GLuint sleeveVbo_ = 0;
    GLuint sleeveEbo_ = 0;

    bool initialized_ = false;
    bool hasHeldBlock_ = false;
    uint32_t heldBlockID_ = 0;

    float swingT_ = 1.0f;
    float pointT_ = 1.0f;
    float idleT_ = 0.0f;

    static constexpr float kSwingDuration = 0.18f;
    static constexpr float kPointDuration = 0.12f;
};
