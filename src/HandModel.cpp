#include "HandModel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "MeshConstants.hpp"
#include "SkinUV.hpp"

namespace {
    constexpr float kPi = 3.14159265359f;

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

    float clamp01(float v) {
        return std::max(0.0f, std::min(1.0f, v));
    }

    float smoothStep01(float x) {
        const float t = clamp01(x);
        return t * t * (3.0f - 2.0f * t);
    }

    void drawMesh(GLuint vao) {
        pglBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }

    std::array<float, 24 * 5> MakeCubeVerticesFromUV(const SkinUV::Cube& cubeUV) {
        std::array<float, 24 * 5> v = MeshConstants::cube_mesh;

        const auto setFaceUV = [&](int faceIndex, const SkinUV::Rect& rect) {
            const float u0 = rect.u0;
            const float u1 = rect.u1;
            const float vt = rect.v0;
            const float vb = rect.v1;

            const std::array<float, 8> uv = {
                u0, vb,
                u1, vb,
                u1, vt,
                u0, vt,
            };

            for (int i = 0; i < 4; ++i) {
                const int vert = faceIndex * 4 + i;
                const int base = vert * 5;
                v[base + 3] = uv[i * 2 + 0];
                v[base + 4] = uv[i * 2 + 1];
            }
        };

        for (int f = 0; f < 6; ++f) {
            setFaceUV(f, cubeUV.faces[static_cast<std::size_t>(f)]);
        }

        return v;
    }
}

HandModel::~HandModel() {
    destroyMesh(armVao_, armVbo_, armEbo_);
    destroyMesh(sleeveVao_, sleeveVbo_, sleeveEbo_);
}

bool HandModel::loadGLFunctions() {
    if (gLoadedGL) {
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

void HandModel::destroyMesh(GLuint& vao, GLuint& vbo, GLuint& ebo) {
    if (ebo != 0 && pglDeleteBuffers) {
        pglDeleteBuffers(1, &ebo);
    }
    if (vbo != 0 && pglDeleteBuffers) {
        pglDeleteBuffers(1, &vbo);
    }
    if (vao != 0 && pglDeleteVertexArrays) {
        pglDeleteVertexArrays(1, &vao);
    }
    vao = 0;
    vbo = 0;
    ebo = 0;
}

bool HandModel::createCubeMeshWithUV(const SkinUV::Cube& cubeUV, GLuint& outVao, GLuint& outVbo, GLuint& outEbo) {
    if (!loadGLFunctions()) {
        return false;
    }

    const std::array<float, 24 * 5> vertices = MakeCubeVerticesFromUV(cubeUV);

    pglGenVertexArrays(1, &outVao);
    pglGenBuffers(1, &outVbo);
    pglGenBuffers(1, &outEbo);

    pglBindVertexArray(outVao);

    pglBindBuffer(GL_ARRAY_BUFFER, outVbo);
    pglBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.data(),
        GL_STATIC_DRAW
    );

    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outEbo);
    pglBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(MeshConstants::kCubeIndices.size() * sizeof(std::uint32_t)),
        MeshConstants::kCubeIndices.data(),
        GL_STATIC_DRAW
    );

    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * static_cast<GLsizei>(sizeof(float)), reinterpret_cast<void*>(0));
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * static_cast<GLsizei>(sizeof(float)),
        reinterpret_cast<void*>(3 * sizeof(float))
    );
    pglEnableVertexAttribArray(1);

    pglBindVertexArray(0);
    return true;
}

bool HandModel::initializeArmMeshes() {
    destroyMesh(armVao_, armVbo_, armEbo_);
    destroyMesh(sleeveVao_, sleeveVbo_, sleeveEbo_);

    const auto skinTables = SkinUV::Minecraft64();
    const auto overlayTables = SkinUV::Minecraft64Overlay();

    if (!createCubeMeshWithUV(skinTables.rightArm, armVao_, armVbo_, armEbo_)) {
        return false;
    }
    if (!createCubeMeshWithUV(overlayTables.rightSleeve, sleeveVao_, sleeveVbo_, sleeveEbo_)) {
        return false;
    }

    return true;
}

bool HandModel::Initialize() {
    if (!heldBlockMesh_.Initialize()) {
        SDL_Log("HandModel: failed to initialize held block mesh.");
        return false;
    }

    if (!initializeArmMeshes()) {
        SDL_Log("HandModel: failed to initialize arm meshes.");
        return false;
    }

    initialized_ = true;
    return true;
}

bool HandModel::LoadSkin(const std::string& skinPath) {
    return skinTexture_.LoadFromFile(skinPath);
}

void HandModel::TriggerSwing() {
    swingT_ = 0.0f;
}

void HandModel::TriggerPoint() {
    pointT_ = 0.0f;
}

void HandModel::Update(float dtSeconds) {
    idleT_ += dtSeconds;

    if (kSwingDuration > 0.0f) {
        swingT_ = std::min(1.0f, swingT_ + dtSeconds / kSwingDuration);
    } else {
        swingT_ = 1.0f;
    }

    if (kPointDuration > 0.0f) {
        pointT_ = std::min(1.0f, pointT_ + dtSeconds / kPointDuration);
    } else {
        pointT_ = 1.0f;
    }
}

void HandModel::SetBlock(uint32_t blockID, const BlockRegistry& registry, const AtlasTexture& atlas) {
    const BlockData* block = registry.Get(blockID);
    if (!block) {
        ClearBlock();
        return;
    }

    heldBlockMesh_.SetFaceTiles(atlas, block->faceTiles);
    heldBlockID_ = blockID;
    hasHeldBlock_ = true;
}

void HandModel::ClearBlock() {
    heldBlockID_ = 0;
    hasHeldBlock_ = false;
}

void HandModel::Draw(
    Shader& blockShader,
    Shader& skinShader,
    const AtlasTexture& blockAtlas,
    int viewportWidth,
    int viewportHeight
) const {
    (void)blockShader;

    if (!initialized_ || viewportWidth <= 0 || viewportHeight <= 0) {
        return;
    }

    const bool canDrawArm = (skinTexture_.TextureID() != 0 && skinShader.Program() != 0);
    if (!canDrawArm && !hasHeldBlock_) {
        return;
    }

    const float aspect = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);
    const glm::mat4 proj = glm::perspective(glm::radians(70.0f), aspect, 0.01f, 8.0f);

    const float swing = smoothStep01(1.0f - swingT_);
    const float point = smoothStep01(1.0f - pointT_);

    const float sqrtSwing = std::sqrt(clamp01(swing));
    const float xSwing = std::sin(sqrtSwing * kPi);
    const float ySwing = std::sin(sqrtSwing * 2.0f * kPi);
    const float zSwing = std::sin(clamp01(swing) * clamp01(swing) * kPi);

    const float idleBob = std::sin(idleT_ * 6.0f) * 0.0125f;

    glm::mat4 root(1.0f);
    root = glm::translate(root, glm::vec3(0.90f - 0.42f * xSwing, -0.52f + idleBob + 0.20f * ySwing, -0.92f - 0.22f * zSwing));
    root = glm::rotate(root, glm::radians(45.0f + 16.0f * xSwing - 50.0f * point), glm::vec3(0.0f, 1.0f, 0.0f));
    root = glm::rotate(root, glm::radians(-80.0f * xSwing - 30.0f * point), glm::vec3(1.0f, 0.0f, 0.0f));
    root = glm::rotate(root, glm::radians(-20.0f * zSwing + 10.0f * point), glm::vec3(0.0f, 0.0f, 1.0f));

    glDisable(GL_CULL_FACE);

    if (canDrawArm) {
        skinShader.Use();
        skinTexture_.Bind(GL_TEXTURE0);
        skinShader.SetInt("uSkin", 0);
        skinShader.SetInt("uShadowMap", 1);
        skinShader.SetInt("uUseSkinning", 0);
        skinShader.SetInt("uJointCount", 0);
        skinShader.SetInt("uReceiveShadows", 0);

        const glm::mat4 identity(1.0f);
        skinShader.SetMat4("uModel", glm::value_ptr(identity));
        skinShader.SetMat4("uLightSpaceMatrix", glm::value_ptr(identity));

        glm::mat4 armModel = root;
        armModel = glm::translate(armModel, glm::vec3(0.26f, -0.22f, -0.30f));
        armModel = glm::rotate(armModel, glm::radians(-26.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        armModel = glm::rotate(armModel, glm::radians(18.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        armModel = glm::rotate(armModel, glm::radians(-12.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        const glm::mat4 armScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.25f, 0.75f, 0.25f));
        const glm::mat4 sleeveScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.2675f, 0.7675f, 0.2675f));

        glm::mat4 mvp = proj * armModel * armScale;
        skinShader.SetMat4("uMVP", glm::value_ptr(mvp));
        drawMesh(armVao_);

        mvp = proj * armModel * sleeveScale;
        skinShader.SetMat4("uMVP", glm::value_ptr(mvp));
        drawMesh(sleeveVao_);
    }

    if (hasHeldBlock_) {
        skinShader.Use();
        blockAtlas.Bind(GL_TEXTURE0);
        skinShader.SetInt("uSkin", 0);
        skinShader.SetInt("uShadowMap", 1);
        skinShader.SetInt("uUseSkinning", 0);
        skinShader.SetInt("uJointCount", 0);
        skinShader.SetInt("uReceiveShadows", 0);

        const glm::mat4 identity(1.0f);
        skinShader.SetMat4("uModel", glm::value_ptr(identity));
        skinShader.SetMat4("uLightSpaceMatrix", glm::value_ptr(identity));

        glm::mat4 heldModel = root;
        heldModel = glm::translate(heldModel, glm::vec3(0.18f, -0.06f, -0.36f));
        heldModel = glm::rotate(heldModel, glm::radians(35.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        heldModel = glm::rotate(heldModel, glm::radians(-20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
        heldModel = glm::scale(heldModel, glm::vec3(0.34f));

        const glm::mat4 heldMvp = proj * heldModel;
        skinShader.SetMat4("uMVP", glm::value_ptr(heldMvp));
        heldBlockMesh_.Draw();
    }

    glEnable(GL_CULL_FACE);
}
