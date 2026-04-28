#include "Grid.hpp"

#include <array>
#include <functional>
#include "MeshConstants.hpp"

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
    PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
    PFNGLBUFFERDATAPROC pglBufferData = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;

    bool gWireframeGlLoaded = false;
    GLuint gWireframeVao = 0;
    GLuint gWireframeVbo = 0;
    GLuint gFaceVao = 0;
    GLuint gFaceVbo = 0;

    void* GLProc(const char* name) {
        return SDL_GL_GetProcAddress(name);
    }

    bool LoadWireframeGLFunctions() {
        if (gWireframeGlLoaded) {
            return true;
        }

        pglGenVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(GLProc("glGenVertexArrays"));
        pglBindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(GLProc("glBindVertexArray"));
        pglDeleteVertexArrays = reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(GLProc("glDeleteVertexArrays"));
        pglGenBuffers = reinterpret_cast<PFNGLGENBUFFERSPROC>(GLProc("glGenBuffers"));
        pglBindBuffer = reinterpret_cast<PFNGLBINDBUFFERPROC>(GLProc("glBindBuffer"));
        pglBufferData = reinterpret_cast<PFNGLBUFFERDATAPROC>(GLProc("glBufferData"));
        pglVertexAttribPointer = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(GLProc("glVertexAttribPointer"));
        pglEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(GLProc("glEnableVertexAttribArray"));

        gWireframeGlLoaded = pglGenVertexArrays && pglBindVertexArray && pglDeleteVertexArrays &&
            pglGenBuffers && pglBindBuffer && pglBufferData &&
            pglVertexAttribPointer && pglEnableVertexAttribArray;
        return gWireframeGlLoaded;
    }

    bool InitializeWireframeMesh() {
        if (gWireframeVao != 0 && gWireframeVbo != 0) {
            return true;
        }
        if (!LoadWireframeGLFunctions()) {
            return false;
        }

        constexpr std::array<float, 72> kWireframeVertices = {
            -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
            0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
            0.5f,  0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
            -0.5f,  0.5f, -0.5f, -0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,  0.5f,
            0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
            0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
            -0.5f, -0.5f, -0.5f, -0.5f, -0.5f,  0.5f,
            0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
            0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
            -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,  0.5f,
        };

        pglGenVertexArrays(1, &gWireframeVao);
        pglGenBuffers(1, &gWireframeVbo);

        pglBindVertexArray(gWireframeVao);
        pglBindBuffer(GL_ARRAY_BUFFER, gWireframeVbo);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(kWireframeVertices), kWireframeVertices.data(), GL_STATIC_DRAW);
        pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
        pglEnableVertexAttribArray(0);
        pglBindBuffer(GL_ARRAY_BUFFER, 0);
        pglBindVertexArray(0);
        return true;
    }

    bool InitializeFaceMesh() {
        if (gFaceVao != 0 && gFaceVbo != 0) {
            return true;
        }
        if (!LoadWireframeGLFunctions()) {
            return false;
        }

        constexpr std::array<float, 72> kFaceQuadVertices = {
            // +X
            0.502f, -0.5f, -0.5f,   0.502f, -0.5f,  0.5f,   0.502f,  0.5f,  0.5f,   0.502f,  0.5f, -0.5f,
            // -X
            -0.502f, -0.5f,  0.5f,  -0.502f, -0.5f, -0.5f,  -0.502f,  0.5f, -0.5f,  -0.502f,  0.5f,  0.5f,
            // +Y
            -0.5f,  0.502f, -0.5f,   0.5f,  0.502f, -0.5f,   0.5f,  0.502f,  0.5f,  -0.5f,  0.502f,  0.5f,
            // -Y
            -0.5f, -0.502f,  0.5f,   0.5f, -0.502f,  0.5f,   0.5f, -0.502f, -0.5f,  -0.5f, -0.502f, -0.5f,
            // +Z
            -0.5f, -0.5f,  0.502f,   0.5f, -0.5f,  0.502f,   0.5f,  0.5f,  0.502f,  -0.5f,  0.5f,  0.502f,
            // -Z
            0.5f, -0.5f, -0.502f,  -0.5f, -0.5f, -0.502f,  -0.5f,  0.5f, -0.502f,   0.5f,  0.5f, -0.502f,
        };

        pglGenVertexArrays(1, &gFaceVao);
        pglGenBuffers(1, &gFaceVbo);
        pglBindVertexArray(gFaceVao);
        pglBindBuffer(GL_ARRAY_BUFFER, gFaceVbo);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(kFaceQuadVertices), kFaceQuadVertices.data(), GL_STATIC_DRAW);
        pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, nullptr);
        pglEnableVertexAttribArray(0);
        pglBindBuffer(GL_ARRAY_BUFFER, 0);
        pglBindVertexArray(0);
        return true;
    }

    bool IntersectRayAabb(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                        const glm::vec3& aabbMin, const glm::vec3& aabbMax,
                        float maxDistance, float& outDistance) {
        float tMin = 0.0f;
        float tMax = maxDistance;

        for (int axis = 0; axis < 3; ++axis) {
            const float origin = rayOrigin[axis];
            const float direction = rayDirection[axis];
            const float slabMin = aabbMin[axis];
            const float slabMax = aabbMax[axis];

            if (glm::abs(direction) < 0.0001f) {
                if (origin < slabMin || origin > slabMax) {
                    return false;
                }
                continue;
            }

            const float inverseDirection = 1.0f / direction;
            float t0 = (slabMin - origin) * inverseDirection;
            float t1 = (slabMax - origin) * inverseDirection;
            if (t0 > t1) {
                const float swapValue = t0;
                t0 = t1;
                t1 = swapValue;
            }

            if (t0 > tMin) {
                tMin = t0;
            }
            if (t1 < tMax) {
                tMax = t1;
            }
            if (tMin > tMax) {
                return false;
            }
        }

        outDistance = tMin;
        return true;
    }

    glm::mat4 WireframeModelMatrix(const glm::ivec3& position) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position));
        model = glm::scale(model, glm::vec3(1.01f, 1.01f, 1.01f));
        return model;
    }
}

size_t Grid::IVec3Hash::operator()(const glm::ivec3& v) const noexcept {
    size_t h = 0;
    h ^= std::hash<int>{}(v.x) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(v.y) + 0x9e3779b9u + (h << 6) + (h >> 2);
    h ^= std::hash<int>{}(v.z) + 0x9e3779b9u + (h << 6) + (h >> 2);
    return h;
}

int Grid::FloorDiv(int a, int b) {
    return a / b - (a % b != 0 && (a ^ b) < 0);
}

glm::ivec3 Grid::ChunkCoord(glm::ivec3 worldPos) {
    return glm::ivec3(FloorDiv(worldPos.x, Chunk::kSize),
                      FloorDiv(worldPos.y, Chunk::kSize),
                      FloorDiv(worldPos.z, Chunk::kSize));
}

glm::ivec3 Grid::LocalPos(glm::ivec3 worldPos, glm::ivec3 chunkCoord) {
    return worldPos - chunkCoord * Chunk::kSize;
}

void Grid::ForEachBlock(const std::function<void(const glm::ivec3&, const FaceTileMap&)>& callback) const {
    for (const auto& [coord, chunk] : chunks_) {
        const glm::ivec3 origin = coord * Chunk::kSize;
        chunk->ForEachBlock([&](int lx, int ly, int lz, const FaceTileMap& tiles) {
            callback(origin + glm::ivec3(lx, ly, lz), tiles);
        });
    }
}

void Grid::MarkNeighborChunksDirty(glm::ivec3 /*worldPos*/, glm::ivec3 chunkCoord, glm::ivec3 localPos) {
    const int dirs[3][2] = {{-1, 0}, {-1, 1}, {-1, 2}};

    auto tryMarkDirty = [&](glm::ivec3 neighborCoord) {
        auto it = chunks_.find(neighborCoord);
        if (it != chunks_.end()) {
            it->second->MarkDirty();
        }
    };

    if (localPos.x == 0)              tryMarkDirty(chunkCoord + glm::ivec3(-1, 0, 0));
    if (localPos.x == Chunk::kSize-1) tryMarkDirty(chunkCoord + glm::ivec3( 1, 0, 0));
    if (localPos.y == 0)              tryMarkDirty(chunkCoord + glm::ivec3( 0,-1, 0));
    if (localPos.y == Chunk::kSize-1) tryMarkDirty(chunkCoord + glm::ivec3( 0, 1, 0));
    if (localPos.z == 0)              tryMarkDirty(chunkCoord + glm::ivec3( 0, 0,-1));
    if (localPos.z == Chunk::kSize-1) tryMarkDirty(chunkCoord + glm::ivec3( 0, 0, 1));

    (void)dirs;  
}

bool Grid::AddBlock(int x, int y, int z, const AtlasTexture& /*atlas*/, const FaceTileMap& faceTiles) {
    const glm::ivec3 worldPos(x, y, z);
    if (HasBlockAt(worldPos)) return false;

    const glm::ivec3 cc = ChunkCoord(worldPos);
    const glm::ivec3 lp = LocalPos(worldPos, cc);

    auto it = chunks_.find(cc);
    if (it == chunks_.end()) {
        it = chunks_.emplace(cc, std::make_unique<Chunk>()).first;
    }
    it->second->SetBlock(lp.x, lp.y, lp.z, faceTiles);
    MarkNeighborChunksDirty(worldPos, cc, lp);
    return true;
}

bool Grid::RemoveBlock(glm::ivec3 pos) {
    const glm::ivec3 cc = ChunkCoord(pos);
    auto it = chunks_.find(cc);
    if (it == chunks_.end()) return false;

    const glm::ivec3 lp = LocalPos(pos, cc);
    if (!it->second->RemoveBlock(lp.x, lp.y, lp.z)) return false;

    MarkNeighborChunksDirty(pos, cc, lp);

    if (it->second->IsEmpty()) {
        chunks_.erase(it);
    }
    return true;
}

void Grid::Clear() {
    chunks_.clear();
}

bool Grid::HasBlockAt(glm::ivec3 pos) const {
    const glm::ivec3 cc = ChunkCoord(pos);
    auto it = chunks_.find(cc);
    if (it == chunks_.end()) return false;
    const glm::ivec3 lp = LocalPos(pos, cc);
    return it->second->HasBlock(lp.x, lp.y, lp.z);
}

void Grid::RebuildVisibility() {
    for (auto& [coord, chunk] : chunks_) {
        chunk->MarkDirty();
    }
}

int Grid::BlockCount() const {
    int total = 0;
    for (const auto& [coord, chunk] : chunks_) {
        total += chunk->BlockCount();
    }
    return total;
}

Grid::LookedAtResult Grid::QueryLookedAt(const Camera& camera, float maxDistance) const {
    return FindLookedAt(camera.position, camera.Forward(), maxDistance);
}

void Grid::Draw(Shader& shader, const AtlasTexture& atlas,
                const glm::mat4& projection, const glm::mat4& view) {
    shader.Use();
    shader.SetInt("uAtlas", 0);
    atlas.Bind(GL_TEXTURE0);

        const float tileW = static_cast<float>(MeshConstants::kTilePixelSize) /
                            static_cast<float>(atlas.Width() > 0 ? atlas.Width() : 1);
        const float tileH = static_cast<float>(MeshConstants::kTilePixelSize) /
                            static_cast<float>(atlas.Height() > 0 ? atlas.Height() : 1);
        shader.SetVec2("uTileSize", tileW, tileH);

    for (auto& [coord, chunk] : chunks_) {
        if (chunk->IsDirty()) {
            const glm::ivec3 origin = coord * Chunk::kSize;
            chunk->RebuildMesh(origin, atlas,
                [this](glm::ivec3 p) { return HasBlockAt(p); });
        }
        const glm::ivec3 origin = coord * Chunk::kSize;
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(origin));
        const glm::mat4 mvp = projection * view * model;
        shader.SetMat4("uMVP", glm::value_ptr(mvp));
        chunk->Draw();
    }
}

Grid::LookedAtResult Grid::FindLookedAt(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                                          float maxDistance) const {
    LookedAtResult nearest;
    float nearestDistance = maxDistance;

    ForEachBlock([&](const glm::ivec3& blockPos, const FaceTileMap& tiles) {
        const glm::vec3 center(blockPos);
        const glm::vec3 aabbMin = center - glm::vec3(0.5f);
        const glm::vec3 aabbMax = center + glm::vec3(0.5f);
        float hitDistance = maxDistance;
        if (!IntersectRayAabb(rayOrigin, rayDirection, aabbMin, aabbMax, maxDistance, hitDistance)) return;
        if (hitDistance > nearestDistance) return;

        nearestDistance = hitDistance;
        nearest.hit = true;
        nearest.blockPos = blockPos;
        nearest.faceTiles = tiles;

        const glm::vec3 hitPoint = rayOrigin + rayDirection * hitDistance;
        const glm::vec3 local = hitPoint - center;
        const float absX = glm::abs(local.x);
        const float absY = glm::abs(local.y);
        const float absZ = glm::abs(local.z);
        if (absX >= absY && absX >= absZ) {
            nearest.faceIndex = (local.x > 0.0f) ? 0 : 1;   // +X or -X
        } else if (absY >= absX && absY >= absZ) {
            nearest.faceIndex = (local.y > 0.0f) ? 2 : 3;   // +Y or -Y
        } else {
            nearest.faceIndex = (local.z > 0.0f) ? 4 : 5;   // +Z or -Z
        }
    });

    return nearest;
}

void Grid::DrawWireframe(Shader& shader,
                         const glm::mat4& projection, const glm::mat4& view) const {
    if (chunks_.empty()) return;

    shader.Use();
    shader.SetVec4("uColor", 0.23f, 0.55f, 1.0f, 1.0f);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(1.0f);

    for (const auto& [coord, chunk] : chunks_) {
        if (chunk->IsEmpty()) continue;
        const glm::ivec3 origin = coord * Chunk::kSize;
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(origin));
        const glm::mat4 mvp = projection * view * model;
        shader.SetMat4("uMVP", glm::value_ptr(mvp));
        chunk->DrawWireframe();
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void Grid::DrawLookedAtBlock(Shader& shader, const Camera& camera,
                              const glm::mat4& projection, const glm::mat4& view) const {
    if (chunks_.empty() || !InitializeWireframeMesh()) {
        return;
    }

    const LookedAtResult hit = FindLookedAt(camera.position, camera.Forward(), 8.0f);
    if (!hit.hit) {
        return;
    }

    const glm::mat4 mvp = projection * view * WireframeModelMatrix(hit.blockPos);
    shader.Use();
    shader.SetMat4("uMVP", glm::value_ptr(mvp));
    shader.SetVec4("uColor", 1.0f, 0.35f, 0.82f, 1.0f);

    pglBindVertexArray(gWireframeVao);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 24);
    glLineWidth(1.0f);
    pglBindVertexArray(0);
}

void Grid::DrawLookedAtFace(Shader& shader, const Camera& camera,
                             const glm::mat4& projection, const glm::mat4& view) const {
    if (chunks_.empty() || !InitializeFaceMesh()) {
        return;
    }


    const LookedAtResult hit = FindLookedAt(camera.position, camera.Forward(), 8.0f);
    if (!hit.hit || hit.faceIndex < 0) {
        return;
    }

    const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(hit.blockPos));
    const glm::mat4 mvp = projection * view * model;
    shader.Use();
    shader.SetMat4("uMVP", glm::value_ptr(mvp));
    shader.SetVec4("uColor", 1.0f, 0.9f, 0.1f, 1.0f);

    pglBindVertexArray(gFaceVao);
    glLineWidth(3.0f);
    glDrawArrays(GL_LINE_LOOP, hit.faceIndex * 4, 4);
    glLineWidth(1.0f);
    pglBindVertexArray(0);
}
