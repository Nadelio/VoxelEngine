#include "Chunk.hpp"

#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include <glm/gtc/type_ptr.hpp>
#include "MeshConstants.hpp"

namespace {
    PFNGLGENVERTEXARRAYSPROC cglGenVertexArrays     = nullptr;
    PFNGLBINDVERTEXARRAYPROC cglBindVertexArray     = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC cglDeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC      cglGenBuffers          = nullptr;
    PFNGLBINDBUFFERPROC      cglBindBuffer          = nullptr;
    PFNGLBUFFERDATAPROC      cglBufferData          = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC    cglVertexAttribPointer   = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC cglEnableVertexAttribArray = nullptr;
    PFNGLDELETEBUFFERSPROC   cglDeleteBuffers       = nullptr;
    bool gChunkGlLoaded = false;
    
    struct FaceInfo {
        int normalAxis;
        int normalDir;
        int uAxis;
        int vAxis;
        bool flipU;
        bool flipV;
    };

    static constexpr FaceInfo kFaceInfo[6] = {
        {2, +1, 0, 1, false, false},  // 0: Front  (+Z)
        {2, -1, 0, 1, true,  false},  // 1: Back   (-Z)
        {0, -1, 2, 1, false, false},  // 2: Left   (-X)
        {0, +1, 2, 1, true,  false},  // 3: Right  (+X)
        {1, +1, 0, 2, false, true },  // 4: Top    (+Y)
        {1, -1, 0, 2, false, false},  // 5: Bottom (-Y)
    };
}

Chunk::~Chunk() {
    if (cglDeleteBuffers && ebo_ != 0) { cglDeleteBuffers(1, &ebo_); }
    if (cglDeleteBuffers && vbo_ != 0) { cglDeleteBuffers(1, &vbo_); }
    if (cglDeleteVertexArrays && vao_ != 0) { cglDeleteVertexArrays(1, &vao_); }
}

bool Chunk::LoadGLFunctions() {
    if (gChunkGlLoaded) return true;

    cglGenVertexArrays      = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>    (SDL_GL_GetProcAddress("glGenVertexArrays"));
    cglBindVertexArray      = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>    (SDL_GL_GetProcAddress("glBindVertexArray"));
    cglDeleteVertexArrays   = reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC> (SDL_GL_GetProcAddress("glDeleteVertexArrays"));
    cglGenBuffers           = reinterpret_cast<PFNGLGENBUFFERSPROC>         (SDL_GL_GetProcAddress("glGenBuffers"));
    cglBindBuffer           = reinterpret_cast<PFNGLBINDBUFFERPROC>         (SDL_GL_GetProcAddress("glBindBuffer"));
    cglBufferData           = reinterpret_cast<PFNGLBUFFERDATAPROC>         (SDL_GL_GetProcAddress("glBufferData"));
    cglVertexAttribPointer  = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(SDL_GL_GetProcAddress("glVertexAttribPointer"));
    cglEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
    cglDeleteBuffers        = reinterpret_cast<PFNGLDELETEBUFFERSPROC>      (SDL_GL_GetProcAddress("glDeleteBuffers"));

    gChunkGlLoaded = cglGenVertexArrays && cglBindVertexArray && cglDeleteVertexArrays &&
                     cglGenBuffers && cglBindBuffer && cglBufferData &&
                     cglVertexAttribPointer && cglEnableVertexAttribArray && cglDeleteBuffers;
    return gChunkGlLoaded;
}

bool Chunk::EnsureGPUResources() {
    if (vao_ != 0) return true;
    if (!LoadGLFunctions()) return false;

    cglGenVertexArrays(1, &vao_);
    cglGenBuffers(1, &vbo_);
    cglGenBuffers(1, &ebo_);

    cglBindVertexArray(vao_);

    cglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    cglBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    cglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    cglBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    cglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
                               7 * static_cast<GLsizei>(sizeof(float)), reinterpret_cast<void*>(0));
    cglEnableVertexAttribArray(0);

    cglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                               7 * static_cast<GLsizei>(sizeof(float)),
                           reinterpret_cast<void*>(3 * sizeof(float)));
    cglEnableVertexAttribArray(1);

        cglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE,
                               7 * static_cast<GLsizei>(sizeof(float)),
                               reinterpret_cast<void*>(5 * sizeof(float)));
        cglEnableVertexAttribArray(2);

    cglBindVertexArray(0);
    return true;
}

bool Chunk::HasBlock(int lx, int ly, int lz) const {
    return blocks_[lx][ly][lz].exists;
}

void Chunk::SetBlock(int lx, int ly, int lz, const FaceTileMap& tiles) {
    if (!blocks_[lx][ly][lz].exists) {
        ++blockCount_;
    }
    blocks_[lx][ly][lz].exists = true;
    blocks_[lx][ly][lz].tiles  = tiles;
    dirty_ = true;
}

bool Chunk::RemoveBlock(int lx, int ly, int lz) {
    if (!blocks_[lx][ly][lz].exists) return false;
    blocks_[lx][ly][lz].exists = false;
    --blockCount_;
    dirty_ = true;
    return true;
}

const FaceTileMap* Chunk::GetTiles(int lx, int ly, int lz) const {
    if (!blocks_[lx][ly][lz].exists) return nullptr;
    return &blocks_[lx][ly][lz].tiles;
}

void Chunk::ForEachBlock(const std::function<void(int, int, int, const FaceTileMap&)>& callback) const {
    for (int lx = 0; lx < kSize; ++lx)
    for (int ly = 0; ly < kSize; ++ly)
    for (int lz = 0; lz < kSize; ++lz) {
        if (blocks_[lx][ly][lz].exists) {
            callback(lx, ly, lz, blocks_[lx][ly][lz].tiles);
        }
    }
}

bool Chunk::RebuildMesh(glm::ivec3 chunkOrigin, const AtlasTexture& atlas,
                         const std::function<bool(glm::ivec3)>& hasBlockAtWorld) {
    std::vector<float>    vertices;
    std::vector<uint32_t> indices;

    const int atlasW = atlas.Width()  > 0 ? atlas.Width()  : 1;
    const int atlasH = atlas.Height() > 0 ? atlas.Height() : 1;
    const float tileW = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(atlasW);
    const float tileH = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(atlasH);

    struct MaskCell {
        bool  exists = false;
        int   tileX  = 0;
        int   tileY  = 0;
        bool operator==(const MaskCell& o) const {
            return exists == o.exists && (!exists || (tileX == o.tileX && tileY == o.tileY));
        }
    };

    MaskCell mask   [kSize][kSize];
    bool     visited[kSize][kSize];

    for (int f = 0; f < 6; ++f) {
        const FaceInfo& fi = kFaceInfo[f];
        const int N = fi.normalAxis;
        const int U = fi.uAxis;
        const int V = fi.vAxis;

        for (int d = 0; d < kSize; ++d) {
            for (int u = 0; u < kSize; ++u) {
            for (int v = 0; v < kSize; ++v) {
                glm::ivec3 lp{};
                lp[N] = d; lp[U] = u; lp[V] = v;

                if (!blocks_[lp.x][lp.y][lp.z].exists) { mask[u][v] = {}; continue; }

                glm::ivec3 np = lp;
                np[N] += fi.normalDir;

                bool neighborSolid;
                if (np.x >= 0 && np.x < kSize &&
                    np.y >= 0 && np.y < kSize &&
                    np.z >= 0 && np.z < kSize) {
                    neighborSolid = blocks_[np.x][np.y][np.z].exists;
                } else {
                    neighborSolid = hasBlockAtWorld(chunkOrigin + np);
                }

                if (neighborSolid) { mask[u][v] = {}; continue; }

                const FaceTile& tile = blocks_[lp.x][lp.y][lp.z].tiles[f];
                mask[u][v] = {true, tile.x, tile.y};
            }}

            memset(visited, 0, sizeof(visited));

            for (int u0 = 0; u0 < kSize; ++u0) {
            for (int v0 = 0; v0 < kSize; ++v0) {
                if (!mask[u0][v0].exists || visited[u0][v0]) continue;

                const MaskCell& cell = mask[u0][v0];

                int W = 1;
                while (u0 + W < kSize && mask[u0 + W][v0] == cell && !visited[u0 + W][v0])
                    ++W;

                int H = 1;
                for (;;) {
                    if (v0 + H >= kSize) break;
                    bool ok = true;
                    for (int w = 0; w < W && ok; ++w)
                        ok = (mask[u0 + w][v0 + H] == cell) && !visited[u0 + w][v0 + H];
                    if (!ok) break;
                    ++H;
                }

                for (int w = 0; w < W; ++w)
                for (int h = 0; h < H; ++h)
                    visited[u0 + w][v0 + h] = true;

                const float tileU0 = static_cast<float>(cell.tileX) * tileW;
                const float tileV0 = static_cast<float>(cell.tileY) * tileH;

                const float uvs[4][2] = {
                    {tileU0,             tileV0 + H * tileH},
                    {tileU0 + W * tileW, tileV0 + H * tileH},
                    {tileU0 + W * tileW, tileV0},
                    {tileU0,             tileV0},
                };

                const float uLow  = static_cast<float>(u0)     - 0.5f;
                const float uHigh = static_cast<float>(u0 + W) - 0.5f;
                const float vLow  = static_cast<float>(v0)     - 0.5f;
                const float vHigh = static_cast<float>(v0 + H) - 0.5f;
                const float dFace = static_cast<float>(d) + fi.normalDir * 0.5f;

                const auto baseIndex = static_cast<uint32_t>(vertices.size() / 7);
                for (int vi = 0; vi < 4; ++vi) {
                    const bool hiU = fi.flipU ? (vi == 0 || vi == 3) : (vi == 1 || vi == 2);
                    const bool hiV = fi.flipV ? (vi == 0 || vi == 1) : (vi == 2 || vi == 3);
                    glm::vec3 pos{};
                    pos[N] = dFace;
                    pos[U] = hiU ? uHigh : uLow;
                    pos[V] = hiV ? vHigh : vLow;
                    vertices.push_back(pos.x);
                    vertices.push_back(pos.y);
                    vertices.push_back(pos.z);
                    vertices.push_back(uvs[vi][0]);
                    vertices.push_back(uvs[vi][1]);
                    vertices.push_back(tileU0);
                    vertices.push_back(tileV0);
                }

                indices.push_back(baseIndex + 0);
                indices.push_back(baseIndex + 1);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 2);
                indices.push_back(baseIndex + 3);
                indices.push_back(baseIndex + 0);
            }}
        }
    }

    if (!EnsureGPUResources()) return false;

    cglBindVertexArray(vao_);

    cglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    cglBufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
                  vertices.empty() ? nullptr : vertices.data(),
                  GL_DYNAMIC_DRAW);

    cglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    cglBufferData(GL_ELEMENT_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                  indices.empty() ? nullptr : indices.data(),
                  GL_DYNAMIC_DRAW);

    indexCount_ = static_cast<int>(indices.size());
    cglBindVertexArray(0);

    dirty_ = false;
    return true;
}

void Chunk::Draw() const {
    if (vao_ == 0 || indexCount_ == 0) return;
    cglBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}

void Chunk::DrawWireframe() const {
    if (vao_ == 0 || indexCount_ == 0) return;
    cglBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
}
