#include "CapeModel.hpp"

#include <array>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

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

    glm::mat4 BuildCapeModelMatrix(
        const glm::mat4& modelMatrix,
        float capeFlap,
        float capeLean,
        float capeLean2
    ) {
        const float degToRad = glm::pi<float>() / 180.0f;

        constexpr float shoulderY = 13.0f;
        const glm::mat4 basePose =
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, shoulderY, 1.0f)) *
            glm::mat4_cast(glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f)));

        const float halfLean = capeLean * 0.5f;
        const float halfLean2 = capeLean2 * 0.5f;
        const float pitchDeg = 6.0f + halfLean + capeFlap;
        const float rollDeg = halfLean2;
        const float finalYawDeg = 180.0f - halfLean2;

        const glm::quat qY0 = glm::angleAxis(-glm::pi<float>(), glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat qX = glm::angleAxis(pitchDeg * degToRad, glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat qZ = glm::angleAxis(rollDeg * degToRad, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::quat qY1 = glm::angleAxis(finalYawDeg * degToRad, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat animRot = qY0 * qX * qZ * qY1;

        return modelMatrix * basePose * glm::mat4_cast(animRot);
    }
}

CapeModel::~CapeModel() {
    if(primitive_.ebo != 0 && pglDeleteBuffers) {
        pglDeleteBuffers(1, &primitive_.ebo);
    }
    if(primitive_.vbo != 0 && pglDeleteBuffers) {
        pglDeleteBuffers(1, &primitive_.vbo);
    }
    if(primitive_.vao != 0 && pglDeleteVertexArrays) {
        pglDeleteVertexArrays(1, &primitive_.vao);
    }
}

bool CapeModel::Initialize() {
    if(!LoadGLFunctions()) {
        SDL_Log("CapeModel: failed to resolve required OpenGL functions.");
        return false;
    }

    if(!CreateCapeMesh()) {
        SDL_Log("CapeModel: failed to create cape mesh.");
        return false;
    }

    initialized_ = true;
    return true;
}

bool CapeModel::LoadCape(const std::string& capePath) {
    return capeTexture_.LoadFromFile(capePath);
}

bool CapeModel::CreateCapeMesh() {
    struct Vertex {
        float x, y, z;
        float u, v;
        float joints[4];
        float weights[4];
    };

    constexpr float texW = 64.0f;
    constexpr float texH = 64.0f;

    // cape mesh dimensions
    constexpr float x0 = -3.0f; // left side of cape
    constexpr float x1 = 3.0f; // right side of cape
    constexpr float y0 = -10.0f; // bottom of cape
    constexpr float y1 = 0.0f; // top of cape
    constexpr float z0 = -1.0f; // front of cape
    constexpr float z1 = 0.0f; // back of cape

    auto uvxCenter = [](float px) { return (px + 0.5f) / texW; };
    auto uvyCenter = [](float py) { return 1.0f - ((py + 0.5f) / texH); };

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(24);
    indices.reserve(36); 

    const float zeroJoints[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float oneWeight[4] = {1.0f, 0.0f, 0.0f, 0.0f};

    auto addVertex = [&](float x, float y, float z, float u, float v) {
        Vertex vert{};
        vert.x = x;
        vert.y = y;
        vert.z = z;
        vert.u = u;
        vert.v = v;
        std::copy(std::begin(zeroJoints), std::end(zeroJoints), std::begin(vert.joints));
        std::copy(std::begin(oneWeight), std::end(oneWeight), std::begin(vert.weights));
        vertices.push_back(vert);
    };

    auto addFace = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                       float u0, float v0, float u1, float v1) {
        const std::uint32_t base = static_cast<std::uint32_t>(vertices.size());
        addVertex(a.x, a.y, a.z, u0, v0);
        addVertex(b.x, b.y, b.z, u1, v0);
        addVertex(c.x, c.y, c.z, u1, v1);
        addVertex(d.x, d.y, d.z, u0, v1);
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    };

    auto addFacePixels = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                             float uMin, float vMin, float uMax, float vMax) {
        float uStart = uMin;
        float uEnd = uMax - 1.0f;
        float vStart = vMin;
        float vEnd = vMax - 1.0f;

        constexpr float inset = 0.5f;
        if((uMax - uMin) > 1.0f) {
            uStart += inset;
            uEnd -= inset;
        }
        if((vMax - vMin) > 1.0f) {
            vStart += inset;
            vEnd -= inset;
        }

        const float u0 = uvxCenter(uStart);
        const float u1 = uvxCenter(uEnd);
        const float v0 = uvyCenter(vStart);
        const float v1 = uvyCenter(vEnd);
        addFace(a, b, c, d, u0, v0, u1, v1);
    };

    // Front (+Z)
    addFacePixels(
        glm::vec3(x0, y1, z1), glm::vec3(x1, y1, z1), glm::vec3(x1, y0, z1), glm::vec3(x0, y0, z1),
        12.0f, 1.0f, 22.0f, 17.0f
    );
    // Back (-Z)
    addFacePixels(
        glm::vec3(x1, y1, z0), glm::vec3(x0, y1, z0), glm::vec3(x0, y0, z0), glm::vec3(x1, y0, z0),
        1.0f, 1.0f, 11.0f, 17.0f
    );
    // Left (-X)
    addFacePixels(
        glm::vec3(x0, y1, z0), glm::vec3(x0, y1, z1), glm::vec3(x0, y0, z1), glm::vec3(x0, y0, z0),
        0.0f, 1.0f, 1.0f, 17.0f
    );
    // Right (+X)
    addFacePixels(
        glm::vec3(x1, y1, z1), glm::vec3(x1, y1, z0), glm::vec3(x1, y0, z0), glm::vec3(x1, y0, z1),
        11.0f, 1.0f, 12.0f, 17.0f
    );
    // Top (+Y)
    addFacePixels(
        glm::vec3(x0, y1, z0), glm::vec3(x1, y1, z0), glm::vec3(x1, y1, z1), glm::vec3(x0, y1, z1),
        1.0f, 0.0f, 11.0f, 1.0f
    );
    // Bottom (-Y)
    addFacePixels(
        glm::vec3(x0, y0, z1), glm::vec3(x1, y0, z1), glm::vec3(x1, y0, z0), glm::vec3(x0, y0, z0),
        11.0f, 0.0f, 21.0f, 1.0f
    );

    pglGenVertexArrays(1, &primitive_.vao);
    pglBindVertexArray(primitive_.vao);

    pglGenBuffers(1, &primitive_.vbo);
    pglBindBuffer(GL_ARRAY_BUFFER, primitive_.vbo);
    pglBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
                  vertices.data(), GL_STATIC_DRAW);

    pglGenBuffers(1, &primitive_.ebo);
    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive_.ebo);
    pglBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(std::uint32_t)),
                  indices.data(), GL_STATIC_DRAW);

    constexpr std::size_t vertexStride = sizeof(Vertex);
    
    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexStride, reinterpret_cast<const void*>(0));
    pglEnableVertexAttribArray(0);

    pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertexStride, reinterpret_cast<const void*>(offsetof(Vertex, u)));
    pglEnableVertexAttribArray(1);

    pglVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, vertexStride, reinterpret_cast<const void*>(offsetof(Vertex, joints)));
    pglEnableVertexAttribArray(2);

    pglVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, vertexStride, reinterpret_cast<const void*>(offsetof(Vertex, weights)));
    pglEnableVertexAttribArray(3);

    pglBindVertexArray(0);

    primitive_.indexCount = static_cast<GLsizei>(indices.size());
    return true;
}

void CapeModel::Draw(
    Shader& capeShader,
    const glm::mat4& projection,
    const glm::mat4& view,
    const glm::mat4& modelMatrix,
    float capeFlap,
    float capeLean,
    float capeLean2
) const {
    if(!initialized_ || !IsLoaded()) {
        return;
    }

    capeShader.Use();

    const glm::mat4 capeModel = BuildCapeModelMatrix(modelMatrix, capeFlap, capeLean, capeLean2);
    glm::mat4 mvp = projection * view * capeModel;

    capeShader.SetInt("uUseSkinning", 0);
    capeShader.SetInt("uJointCount", 0);
    capeShader.SetMat4("uMVP", glm::value_ptr(mvp));
    capeShader.SetMat4("uModel", glm::value_ptr(capeModel));

    capeTexture_.Bind(GL_TEXTURE0);
    capeShader.SetInt("uSkin", 0);

    pglBindVertexArray(primitive_.vao);
    glDrawElements(GL_TRIANGLES, primitive_.indexCount, GL_UNSIGNED_INT, nullptr);
    pglBindVertexArray(0);
}

void CapeModel::DrawShadow(
    Shader& shadowShader,
    const glm::mat4& lightSpaceMatrix,
    const glm::mat4& modelMatrix,
    float capeFlap,
    float capeLean,
    float capeLean2
) const {
    if(!initialized_ || !IsLoaded()) {
        return;
    }

    shadowShader.Use();

    const glm::mat4 capeModel = BuildCapeModelMatrix(modelMatrix, capeFlap, capeLean, capeLean2);
    glm::mat4 mvp = lightSpaceMatrix * capeModel;

    shadowShader.SetInt("uUseSkinning", 0);
    shadowShader.SetInt("uJointCount", 0);
    shadowShader.SetMat4("uLightMVP", glm::value_ptr(mvp));

    pglBindVertexArray(primitive_.vao);
    glDrawElements(GL_TRIANGLES, primitive_.indexCount, GL_UNSIGNED_INT, nullptr);
    pglBindVertexArray(0);
}
