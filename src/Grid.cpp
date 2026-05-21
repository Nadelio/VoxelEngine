#include "Grid.hpp"

#include <array>
#include <cmath>
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
    PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;
    PFNGLBUFFERDATAPROC pglBufferData = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;

    bool gWireframeGlLoaded = false;
    GLuint gWireframeVao = 0;
    GLuint gWireframeVbo = 0;
    GLuint gFaceVao = 0;
    GLuint gFaceVbo = 0;
    GLuint gFallVao = 0;
    GLuint gFallVbo = 0;
    GLuint gFallEbo = 0;

    SDL_FunctionPointer GLProc(const char* name) {
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
        pglDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(GLProc("glDeleteBuffers"));
        pglBufferData = reinterpret_cast<PFNGLBUFFERDATAPROC>(GLProc("glBufferData"));
        pglVertexAttribPointer = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(GLProc("glVertexAttribPointer"));
        pglEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(GLProc("glEnableVertexAttribArray"));

        gWireframeGlLoaded = pglGenVertexArrays && pglBindVertexArray && pglDeleteVertexArrays &&
            pglGenBuffers && pglBindBuffer && pglDeleteBuffers && pglBufferData &&
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

    bool InitFallMesh() {
        if (gFallVao != 0) return true;
        if (!LoadWireframeGLFunctions()) return false;

        constexpr std::array<uint32_t, 36> kIdx = {
             0,  1,  2,   2,  3,  0,
             4,  5,  6,   6,  7,  4,
             8,  9, 10,  10, 11,  8,
            12, 13, 14,  14, 15, 12,
            16, 17, 18,  18, 19, 16,
            20, 21, 22,  22, 23, 20,
        };

        pglGenVertexArrays(1, &gFallVao);
        pglGenBuffers(1, &gFallVbo);
        pglGenBuffers(1, &gFallEbo);

        pglBindVertexArray(gFallVao);
        pglBindBuffer(GL_ARRAY_BUFFER, gFallVbo);
        pglBufferData(GL_ARRAY_BUFFER, 24 * 10 * static_cast<GLsizeiptr>(sizeof(float)),
                      nullptr, GL_DYNAMIC_DRAW);
        pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gFallEbo);
        pglBufferData(GL_ELEMENT_ARRAY_BUFFER,
                      static_cast<GLsizeiptr>(sizeof(kIdx)), kIdx.data(), GL_STATIC_DRAW);
        
        pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 10 * static_cast<GLsizei>(sizeof(float)),
                               reinterpret_cast<void*>(0));
        pglEnableVertexAttribArray(0);
        pglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 10 * static_cast<GLsizei>(sizeof(float)),
                               reinterpret_cast<void*>(3 * sizeof(float)));
        pglEnableVertexAttribArray(1);
        pglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 10 * static_cast<GLsizei>(sizeof(float)),
                               reinterpret_cast<void*>(5 * sizeof(float)));
        pglEnableVertexAttribArray(2);
        pglVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 10 * static_cast<GLsizei>(sizeof(float)),
                       reinterpret_cast<void*>(7 * sizeof(float)));
        pglEnableVertexAttribArray(3);
        pglBindVertexArray(0);
        return true;
    }

    [[maybe_unused]] bool IntersectRayAabb(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
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

bool Grid::HasAnyBlockInRange(glm::ivec3 minPos, glm::ivec3 maxPos) const {
    const glm::ivec3 minChunk = ChunkCoord(minPos);
    const glm::ivec3 maxChunk = ChunkCoord(maxPos);
    for (int cx = minChunk.x; cx <= maxChunk.x; ++cx) {
        for (int cy = minChunk.y; cy <= maxChunk.y; ++cy) {
            for (int cz = minChunk.z; cz <= maxChunk.z; ++cz) {
                const glm::ivec3 cc{cx, cy, cz};
                auto it = chunks_.find(cc);
                if (it == chunks_.end()) continue;
                const glm::ivec3 origin = cc * Chunk::kSize;
                const int lxMin = std::max(minPos.x - origin.x, 0);
                const int lyMin = std::max(minPos.y - origin.y, 0);
                const int lzMin = std::max(minPos.z - origin.z, 0);
                const int lxMax = std::min(maxPos.x - origin.x, Chunk::kSize - 1);
                const int lyMax = std::min(maxPos.y - origin.y, Chunk::kSize - 1);
                const int lzMax = std::min(maxPos.z - origin.z, Chunk::kSize - 1);
                for (int lx = lxMin; lx <= lxMax; ++lx)
                    for (int ly = lyMin; ly <= lyMax; ++ly)
                        for (int lz = lzMin; lz <= lzMax; ++lz)
                            if (it->second->HasBlock(lx, ly, lz))
                                return true;
            }
        }
    }
    return false;
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

bool Grid::AddBlock(int x, int y, int z, uint32_t blockID, uint8_t rotation) {
    if (registry_ && !registry_->Get(blockID)) {
        return false;
    }

    const glm::ivec3 worldPos(x, y, z);
    if (HasBlockAt(worldPos)) return false;

    const glm::ivec3 cc = ChunkCoord(worldPos);
    const glm::ivec3 lp = LocalPos(worldPos, cc);

    auto it = chunks_.find(cc);
    if (it == chunks_.end()) {
        it = chunks_.emplace(cc, std::make_unique<Chunk>()).first;
    }
    it->second->SetBlock(lp.x, lp.y, lp.z, blockID, rotation);
    it->second->MarkDirty();
    MarkNeighborChunksDirty(worldPos, cc, lp);
    if (registry_) {
        const BlockData* bd = registry_->Get(blockID);
        if (bd && bd->affectedByGravity)
            gravityBlocks_.insert(worldPos);
    }
    return true;
}

void Grid::AddBlockBulk(int x, int y, int z, uint32_t blockID, uint8_t rotation) {
    const glm::ivec3 worldPos(x, y, z);
    const glm::ivec3 cc = ChunkCoord(worldPos);
    const glm::ivec3 lp = LocalPos(worldPos, cc);

    auto it = chunks_.find(cc);
    if (it == chunks_.end()) {
        it = chunks_.emplace(cc, std::make_unique<Chunk>()).first;
    }
    it->second->SetBlock(lp.x, lp.y, lp.z, blockID, rotation);
    if (registry_) {
        const BlockData* bd = registry_->Get(blockID);
        if (bd && bd->affectedByGravity)
            gravityBlocks_.insert(worldPos);
    }
}

bool Grid::RemoveBlock(glm::ivec3 pos) {
    const glm::ivec3 cc = ChunkCoord(pos);
    auto it = chunks_.find(cc);
    if (it == chunks_.end()) return false;

    const glm::ivec3 lp = LocalPos(pos, cc);
    if (!it->second->RemoveBlock(lp.x, lp.y, lp.z)) return false;

    it->second->MarkDirty();
    MarkNeighborChunksDirty(pos, cc, lp);
    gravityBlocks_.erase(pos);

    // Re-activate any gravity block that was resting on this one.
    if (registry_) {
        const glm::ivec3 above = pos + glm::ivec3(0, 1, 0);
        const uint32_t aboveID = GetBlockID(above);
        if (aboveID != 0) {
            const BlockData* bd = registry_->Get(aboveID);
            if (bd && bd->affectedByGravity)
                gravityBlocks_.insert(above);
        }
    }

    if (it->second->IsEmpty()) {
        chunks_.erase(it);
    }
    return true;
}

void Grid::Clear() {
    chunks_.clear();
    gravityBlocks_.clear();
}

bool Grid::HasBlockAt(glm::ivec3 pos) const {
    const glm::ivec3 cc = ChunkCoord(pos);
    auto it = chunks_.find(cc);
    if (it == chunks_.end()) return false;
    const glm::ivec3 lp = LocalPos(pos, cc);
    return it->second->HasBlock(lp.x, lp.y, lp.z);
}

uint32_t Grid::GetBlockID(glm::ivec3 pos) const {
    const glm::ivec3 cc = ChunkCoord(pos);
    auto it = chunks_.find(cc);
    if (it == chunks_.end()) return 0;
    const glm::ivec3 lp = LocalPos(pos, cc);
    if (!it->second->HasBlock(lp.x, lp.y, lp.z)) return 0;
    return it->second->GetBlockID(lp.x, lp.y, lp.z);
}

void Grid::RebuildVisibility() {
    for (auto& [coord, chunk] : chunks_) {
        chunk->MarkDirty();
    }
}

void Grid::RebuildAll(const AtlasTexture& atlas) {
    if (!registry_) return;
    for (auto& [coord, chunk] : chunks_) {
        const glm::ivec3 origin = coord * Chunk::kSize;
        chunk->MarkDirty();
        chunk->RebuildMesh(origin, atlas, *registry_,
            [this](glm::ivec3 p) { return HasBlockAt(p); });
    }
}

int Grid::BlockCount() const {
    int total = 0;
    for (const auto& [coord, chunk] : chunks_) {
        total += chunk->BlockCount();
    }
    return total;
}

void Grid::VisitBlocks(const std::function<void(const glm::ivec3&, uint32_t)>& callback) const {
    ForEachBlock(callback);
}

uint8_t Grid::GetBlockRotation(glm::ivec3 pos) const {
    const glm::ivec3 cc = ChunkCoord(pos);
    const auto it = chunks_.find(cc);
    if (it == chunks_.end()) return 0;
    const glm::ivec3 lp = LocalPos(pos, cc);
    if (!it->second->HasBlock(lp.x, lp.y, lp.z)) return 0;
    return it->second->GetBlockRotation(lp.x, lp.y, lp.z);
}

void Grid::ReleaseSharedGLResources() {
    if (!LoadWireframeGLFunctions()) {
        return;
    }

    if (gWireframeVbo != 0) {
        pglDeleteBuffers(1, &gWireframeVbo);
        gWireframeVbo = 0;
    }
    if (gWireframeVao != 0) {
        pglDeleteVertexArrays(1, &gWireframeVao);
        gWireframeVao = 0;
    }

    if (gFaceVbo != 0) {
        pglDeleteBuffers(1, &gFaceVbo);
        gFaceVbo = 0;
    }
    if (gFaceVao != 0) {
        pglDeleteVertexArrays(1, &gFaceVao);
        gFaceVao = 0;
    }

    if (gFallEbo != 0) {
        pglDeleteBuffers(1, &gFallEbo);
        gFallEbo = 0;
    }
    if (gFallVbo != 0) {
        pglDeleteBuffers(1, &gFallVbo);
        gFallVbo = 0;
    }
    if (gFallVao != 0) {
        pglDeleteVertexArrays(1, &gFallVao);
        gFallVao = 0;
    }
}

Grid::LookedAtResult Grid::QueryLookedAt(const Camera& camera, float maxDistance) const {
    return FindLookedAt(camera.position, camera.Forward(), maxDistance);
}

namespace {
    struct Plane { float nx, ny, nz, d; };

    std::array<Plane, 6> ExtractFrustumPlanes(const glm::mat4& pv) {
        auto row = [&](int i) -> glm::vec4 {
            return { pv[0][i], pv[1][i], pv[2][i], pv[3][i] };
        };
        auto toPlane = [](glm::vec4 v) -> Plane {
            const float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
            return { v.x/len, v.y/len, v.z/len, v.w/len };
        };
        const glm::vec4 r0 = row(0), r1 = row(1), r2 = row(2), r3 = row(3);
        return {{
            toPlane(r3 + r0),  // left
            toPlane(r3 - r0),  // right
            toPlane(r3 + r1),  // bottom
            toPlane(r3 - r1),  // top
            toPlane(r3 + r2),  // near
            toPlane(r3 - r2),  // far
        }};
    }

    bool ChunkInFrustum(const std::array<Plane, 6>& planes, const glm::ivec3& coord) {
        const glm::vec3 minPt = glm::vec3(coord * Chunk::kSize);
        const glm::vec3 maxPt = minPt + glm::vec3(Chunk::kSize);
        constexpr float kFrustumPadding = 2.0f;
        for (const auto& p : planes) {
            const float px = (p.nx >= 0.0f) ? maxPt.x : minPt.x;
            const float py = (p.ny >= 0.0f) ? maxPt.y : minPt.y;
            const float pz = (p.nz >= 0.0f) ? maxPt.z : minPt.z;
            if (p.nx*px + p.ny*py + p.nz*pz + p.d < -kFrustumPadding)
                return false;
        }
        return true;
    }
}

void Grid::Draw(Shader& shader, const AtlasTexture& atlas,
                const glm::mat4& projection, const glm::mat4& view,
                const glm::mat4& lightSpaceMatrix) {
    if (!registry_) return;

    shader.Use();
    shader.SetInt("uAtlas", 0);
    atlas.Bind(GL_TEXTURE0);

    const float tileW = static_cast<float>(MeshConstants::kTilePixelSize) /
                        static_cast<float>(atlas.Width() > 0 ? atlas.Width() : 1);
    const float tileH = static_cast<float>(MeshConstants::kTilePixelSize) /
                        static_cast<float>(atlas.Height() > 0 ? atlas.Height() : 1);
    shader.SetVec2("uTileSize", tileW, tileH);
    shader.SetInt("uShadowMap", 1);
    shader.SetMat4("uLightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));

    const glm::mat4 pv     = projection * view;
    const auto      planes = ExtractFrustumPlanes(pv);

    for (auto& [coord, chunk] : chunks_) {
        if (chunk->IsDirty()) {
            const glm::ivec3 origin = coord * Chunk::kSize;
            chunk->RebuildMesh(origin, atlas, *registry_,
                [this](glm::ivec3 p) { return HasBlockAt(p); });
        }
        if (chunk->IndexCount() == 0) continue;
        if (!ChunkInFrustum(planes, coord)) continue;

        const glm::ivec3 origin = coord * Chunk::kSize;
        const glm::mat4 model  = glm::translate(glm::mat4(1.0f), glm::vec3(origin));
        const glm::mat4 mvp    = pv * model;
        shader.SetMat4("uMVP", glm::value_ptr(mvp));
        shader.SetMat4("uModel", glm::value_ptr(model));
        chunk->Draw();
    }
}

void Grid::DrawShadowMap(Shader& shader, const AtlasTexture& atlas, const glm::mat4& lightSpaceMatrix) {
    if (!registry_) return;

    shader.Use();

    for (auto& [coord, chunk] : chunks_) {
        if (chunk->IsDirty()) {
            const glm::ivec3 origin = coord * Chunk::kSize;
            chunk->RebuildMesh(origin, atlas, *registry_,
                [this](glm::ivec3 p) { return HasBlockAt(p); });
        }
        if (chunk->IndexCount() == 0) continue;

        const glm::ivec3 origin = coord * Chunk::kSize;
        const glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(origin));
        const glm::mat4 lightMvp = lightSpaceMatrix * model;
        shader.SetMat4("uLightMVP", glm::value_ptr(lightMvp));
        chunk->DrawShadow();
    }
}

Grid::LookedAtResult Grid::FindLookedAt(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                                          float maxDistance) const {
    LookedAtResult result;

    const glm::vec3 o = rayOrigin + glm::vec3(0.5f);

    int ix = static_cast<int>(std::floor(o.x));
    int iy = static_cast<int>(std::floor(o.y));
    int iz = static_cast<int>(std::floor(o.z));

    const int stepX = (rayDirection.x >= 0.0f) ? 1 : -1;
    const int stepY = (rayDirection.y >= 0.0f) ? 1 : -1;
    const int stepZ = (rayDirection.z >= 0.0f) ? 1 : -1;

    const float tDeltaX = (rayDirection.x != 0.0f) ? std::abs(1.0f / rayDirection.x) : 1e30f;
    const float tDeltaY = (rayDirection.y != 0.0f) ? std::abs(1.0f / rayDirection.y) : 1e30f;
    const float tDeltaZ = (rayDirection.z != 0.0f) ? std::abs(1.0f / rayDirection.z) : 1e30f;

    const float fxo = o.x - std::floor(o.x);
    const float fyo = o.y - std::floor(o.y);
    const float fzo = o.z - std::floor(o.z);
    float tMaxX = (stepX > 0) ? (1.0f - fxo) * tDeltaX : fxo * tDeltaX;
    float tMaxY = (stepY > 0) ? (1.0f - fyo) * tDeltaY : fyo * tDeltaY;
    float tMaxZ = (stepZ > 0) ? (1.0f - fzo) * tDeltaZ : fzo * tDeltaZ;

    int lastAxis = -1;

    for (;;) {
        const glm::ivec3 pos{ix, iy, iz};
        if (HasBlockAt(pos)) {
            result.hit      = true;
            result.blockPos = pos;
            result.blockID  = GetBlockID(pos);
            result.blockData = registry_ ? registry_->Get(result.blockID) : nullptr;

            switch (lastAxis) {
                case 0: result.faceIndex = (stepX > 0) ? 1 : 0; break;
                case 1: result.faceIndex = (stepY > 0) ? 3 : 2; break;
                case 2: result.faceIndex = (stepZ > 0) ? 5 : 4; break;
                default: {
                    const glm::vec3 lp = rayOrigin - glm::vec3(pos);
                    const float ax = std::abs(lp.x), ay = std::abs(lp.y), az = std::abs(lp.z);
                    if      (ax >= ay && ax >= az) result.faceIndex = (lp.x > 0.0f) ? 0 : 1;
                    else if (ay >= az)             result.faceIndex = (lp.y > 0.0f) ? 2 : 3;
                    else                           result.faceIndex = (lp.z > 0.0f) ? 4 : 5;
                    break;
                }
            }
            return result;
        }

        if (tMaxX < tMaxY && tMaxX < tMaxZ) {
            if (tMaxX > maxDistance) break;
            ix += stepX;
            tMaxX += tDeltaX;
            lastAxis = 0;
        } else if (tMaxY < tMaxZ) {
            if (tMaxY > maxDistance) break;
            iy += stepY;
            tMaxY += tDeltaY;
            lastAxis = 1;
        } else {
            if (tMaxZ > maxDistance) break;
            iz += stepZ;
            tMaxZ += tDeltaZ;
            lastAxis = 2;
        }
    }

    return result;
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

void Grid::DrawFloatBlocks(const std::vector<FloatBlock>& blocks,
                           Shader& shader, const AtlasTexture& atlas,
                           const glm::mat4& projection, const glm::mat4& view,
                           const glm::mat4& lightSpaceMatrix) {
    if (blocks.empty() || !registry_) return;
    if (!InitFallMesh()) return;

    shader.Use();
    shader.SetInt("uAtlas", 0);
    atlas.Bind(GL_TEXTURE0);

    const float tileW = static_cast<float>(MeshConstants::kTilePixelSize) /
                        static_cast<float>(atlas.Width() > 0 ? atlas.Width() : 1);
    const float tileH = static_cast<float>(MeshConstants::kTilePixelSize) /
                        static_cast<float>(atlas.Height() > 0 ? atlas.Height() : 1);
    shader.SetVec2("uTileSize", tileW, tileH);
    shader.SetInt("uShadowMap", 1);
    shader.SetMat4("uLightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));

    struct FaceLayout { int N, normalDir, U, V; bool flipU, flipV; };
    static constexpr FaceLayout kFaces[6] = {
        {2, +1, 0, 1, false, false},  // Front  (+Z)
        {2, -1, 0, 1, true,  false},  // Back   (-Z)
        {0, -1, 2, 1, false, false},  // Left   (-X)
        {0, +1, 2, 1, true,  false},  // Right  (+X)
        {1, +1, 0, 2, false, true },  // Top    (+Y)
        {1, -1, 0, 2, false, false},  // Bottom (-Y)
    };

    pglBindVertexArray(gFallVao);

    for (const FloatBlock& fb : blocks) {
        const BlockData* data = registry_->Get(fb.blockID);
        if (!data) continue;

        float verts[24 * 10];
        int idx = 0;

        for (int f = 0; f < 6; ++f) {
            const FaceLayout& fi = kFaces[f];
            const FaceTile& tile = data->faceTiles[static_cast<size_t>(f)];
            const float tU0 = tile.x * tileW;
            const float tV0 = tile.y * tileH;
            const float uvCorners[4][2] = {
                {tU0,        tV0 + tileH},
                {tU0 + tileW, tV0 + tileH},
                {tU0 + tileW, tV0        },
                {tU0,        tV0        },
            };
            const float dFace = fi.normalDir * 0.5f;

            for (int v = 0; v < 4; ++v) {
                const bool hiU = fi.flipU ? (v == 0 || v == 3) : (v == 1 || v == 2);
                const bool hiV = fi.flipV ? (v == 0 || v == 1) : (v == 2 || v == 3);
                glm::vec3 pos(0.0f);
                pos[fi.N] = dFace;
                pos[fi.U] = hiU ? 0.5f : -0.5f;
                pos[fi.V] = hiV ? 0.5f : -0.5f;
                verts[idx++] = pos.x;
                verts[idx++] = pos.y;
                verts[idx++] = pos.z;
                verts[idx++] = uvCorners[v][0];
                verts[idx++] = uvCorners[v][1];
                verts[idx++] = tU0;
                verts[idx++] = tV0;
                glm::vec3 normal(0.0f);
                normal[fi.N] = static_cast<float>(fi.normalDir);
                verts[idx++] = normal.x;
                verts[idx++] = normal.y;
                verts[idx++] = normal.z;
            }
        }

        pglBindBuffer(GL_ARRAY_BUFFER, gFallVbo);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

        const glm::mat4 model = glm::translate(glm::mat4(1.0f), fb.pos);
        const glm::mat4 mvp   = projection * view * model;
        shader.SetMat4("uMVP", glm::value_ptr(mvp));
        shader.SetMat4("uModel", glm::value_ptr(model));

        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, reinterpret_cast<void*>(0));
    }

    pglBindVertexArray(0);
}

void Grid::DrawFloatBlocksShadow(const std::vector<FloatBlock>& blocks,
                                 Shader& shadowShader,
                                 const glm::mat4& lightSpaceMatrix) {
    if (blocks.empty()) return;
    if (!InitFallMesh()) return;

    struct FaceLayout { int N, normalDir, U, V; bool flipU, flipV; };
    static constexpr FaceLayout kFaces[6] = {
        {2, +1, 0, 1, false, false},  // Front  (+Z)
        {2, -1, 0, 1, true,  false},  // Back   (-Z)
        {0, -1, 2, 1, false, false},  // Left   (-X)
        {0, +1, 2, 1, true,  false},  // Right  (+X)
        {1, +1, 0, 2, false, true },  // Top    (+Y)
        {1, -1, 0, 2, false, false},  // Bottom (-Y)
    };

    shadowShader.Use();
    pglBindVertexArray(gFallVao);

    for (const FloatBlock& fb : blocks) {
        float verts[24 * 10];
        int idx = 0;

        for (int f = 0; f < 6; ++f) {
            const FaceLayout& fi = kFaces[f];
            const float dFace = fi.normalDir * 0.5f;

            for (int v = 0; v < 4; ++v) {
                const bool hiU = fi.flipU ? (v == 0 || v == 3) : (v == 1 || v == 2);
                const bool hiV = fi.flipV ? (v == 0 || v == 1) : (v == 2 || v == 3);
                glm::vec3 pos(0.0f);
                pos[fi.N] = dFace;
                pos[fi.U] = hiU ? 0.5f : -0.5f;
                pos[fi.V] = hiV ? 0.5f : -0.5f;
                verts[idx++] = pos.x;
                verts[idx++] = pos.y;
                verts[idx++] = pos.z;
                verts[idx++] = 0.0f;
                verts[idx++] = 0.0f;
                verts[idx++] = 0.0f;
                verts[idx++] = 0.0f;
                glm::vec3 normal(0.0f);
                normal[fi.N] = static_cast<float>(fi.normalDir);
                verts[idx++] = normal.x;
                verts[idx++] = normal.y;
                verts[idx++] = normal.z;
            }
        }

        pglBindBuffer(GL_ARRAY_BUFFER, gFallVbo);
        pglBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);

        const glm::mat4 model = glm::translate(glm::mat4(1.0f), fb.pos);
        const glm::mat4 lightMvp = lightSpaceMatrix * model;
        shadowShader.SetMat4("uLightMVP", glm::value_ptr(lightMvp));

        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, reinterpret_cast<void*>(0));
    }

    pglBindVertexArray(0);
}
