#include "FluidGrid.hpp"

#include <algorithm>
#include <cstring>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <SDL3/SDL.h>
#include <glm/gtc/type_ptr.hpp>

#include "MeshConstants.hpp"

namespace {
    PFNGLGENVERTEXARRAYSPROC      fglGenVertexArrays       = nullptr;
    PFNGLBINDVERTEXARRAYPROC      fglBindVertexArray       = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC   fglDeleteVertexArrays    = nullptr;
    PFNGLGENBUFFERSPROC           fglGenBuffers            = nullptr;
    PFNGLBINDBUFFERPROC           fglBindBuffer            = nullptr;
    PFNGLBUFFERDATAPROC           fglBufferData            = nullptr;
    PFNGLBUFFERSUBDATAPROC        fglBufferSubData         = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC  fglVertexAttribPointer   = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC fglEnableVertexAttribArray = nullptr;
    PFNGLDELETEBUFFERSPROC        fglDeleteBuffers         = nullptr;
    bool gFluidGLLoaded = false;
}

bool FluidGrid::LoadGLFunctions() {
    if (gFluidGLLoaded) return true;
    fglGenVertexArrays        = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>       (SDL_GL_GetProcAddress("glGenVertexArrays"));
    fglBindVertexArray        = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>       (SDL_GL_GetProcAddress("glBindVertexArray"));
    fglDeleteVertexArrays     = reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>    (SDL_GL_GetProcAddress("glDeleteVertexArrays"));
    fglGenBuffers             = reinterpret_cast<PFNGLGENBUFFERSPROC>            (SDL_GL_GetProcAddress("glGenBuffers"));
    fglBindBuffer             = reinterpret_cast<PFNGLBINDBUFFERPROC>            (SDL_GL_GetProcAddress("glBindBuffer"));
    fglBufferData             = reinterpret_cast<PFNGLBUFFERDATAPROC>            (SDL_GL_GetProcAddress("glBufferData"));
    fglBufferSubData          = reinterpret_cast<PFNGLBUFFERSUBDATAPROC>         (SDL_GL_GetProcAddress("glBufferSubData"));
    fglVertexAttribPointer    = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>   (SDL_GL_GetProcAddress("glVertexAttribPointer"));
    fglEnableVertexAttribArray= reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
    fglDeleteBuffers          = reinterpret_cast<PFNGLDELETEBUFFERSPROC>         (SDL_GL_GetProcAddress("glDeleteBuffers"));
    gFluidGLLoaded = fglGenVertexArrays && fglBindVertexArray && fglDeleteVertexArrays &&
                     fglGenBuffers && fglBindBuffer && fglBufferData && fglBufferSubData &&
                     fglVertexAttribPointer && fglEnableVertexAttribArray && fglDeleteBuffers;
    return gFluidGLLoaded;
}

FluidGrid::~FluidGrid() {
    for (auto& [pos, chunk] : chunks_) {
        if (fglDeleteBuffers) {
            if (chunk.ebo) fglDeleteBuffers(1, &chunk.ebo);
            if (chunk.vbo) fglDeleteBuffers(1, &chunk.vbo);
        }
        if (fglDeleteVertexArrays && chunk.vao)
            fglDeleteVertexArrays(1, &chunk.vao);
    }
}

static inline int FloorDiv(int a, int b) {
    return a / b - (a % b != 0 && (a ^ b) < 0);
}

glm::ivec3 FluidGrid::WorldToChunk(glm::ivec3 pos) const {
    return { FloorDiv(pos.x, kChunkSize),
             FloorDiv(pos.y, kChunkSize),
             FloorDiv(pos.z, kChunkSize) };
}

glm::ivec3 FluidGrid::WorldToLocal(glm::ivec3 pos) const {
    auto mod = [](int a, int b) -> int { return ((a % b) + b) % b; };
    return { mod(pos.x, kChunkSize), mod(pos.y, kChunkSize), mod(pos.z, kChunkSize) };
}

FluidGrid::FluidChunk& FluidGrid::GetOrCreateChunk(glm::ivec3 cp) {
    return chunks_[cp];
}

FluidGrid::FluidChunk* FluidGrid::GetChunk(glm::ivec3 cp) {
    auto it = chunks_.find(cp);
    return it == chunks_.end() ? nullptr : &it->second;
}

const FluidGrid::FluidChunk* FluidGrid::GetChunk(glm::ivec3 cp) const {
    auto it = chunks_.find(cp);
    return it == chunks_.end() ? nullptr : &it->second;
}

uint8_t FluidGrid::GetLevelAtWorld(int x, int y, int z) const {
    const glm::ivec3 wp{x, y, z};
    const FluidChunk* c = GetChunk(WorldToChunk(wp));
    if (!c) return 0;
    const glm::ivec3 lp = WorldToLocal(wp);
    return c->cells[lp.x][lp.y][lp.z].exists ? c->cells[lp.x][lp.y][lp.z].level : 0;
}

uint32_t FluidGrid::GetIDAtWorld(int x, int y, int z) const {
    const glm::ivec3 wp{x, y, z};
    const FluidChunk* c = GetChunk(WorldToChunk(wp));
    if (!c) return 0;
    const glm::ivec3 lp = WorldToLocal(wp);
    return c->cells[lp.x][lp.y][lp.z].exists ? c->cells[lp.x][lp.y][lp.z].fluidID : 0;
}

bool FluidGrid::HasFluid(int x, int y, int z) const {
    return GetLevelAtWorld(x, y, z) > 0;
}

uint32_t FluidGrid::GetFluidID(int x, int y, int z) const {
    return GetIDAtWorld(x, y, z);
}

uint8_t FluidGrid::GetFluidLevel(int x, int y, int z) const {
    return GetLevelAtWorld(x, y, z);
}

void FluidGrid::SetFluid(int x, int y, int z, uint32_t fluidID, uint8_t level, bool isSource) {
    const glm::ivec3 wp{x, y, z};
    FluidChunk& c = GetOrCreateChunk(WorldToChunk(wp));
    const glm::ivec3 lp = WorldToLocal(wp);
    FluidCell& cell = c.cells[lp.x][lp.y][lp.z];
    if (!cell.exists) ++c.cellCount;
    cell.exists   = true;
    cell.fluidID  = fluidID;
    cell.level    = level;
    cell.isSource = isSource;
    c.dirty = true;
}

void FluidGrid::SetFluidBulk(int x, int y, int z, uint32_t fluidID, uint8_t level, bool isSource) {
    const glm::ivec3 wp{x, y, z};
    FluidChunk& c = GetOrCreateChunk(WorldToChunk(wp));
    const glm::ivec3 lp = WorldToLocal(wp);
    FluidCell& cell = c.cells[lp.x][lp.y][lp.z];
    if (!cell.exists) ++c.cellCount;
    cell.exists   = true;
    cell.fluidID  = fluidID;
    cell.level    = level;
    cell.isSource = isSource;
}

void FluidGrid::RemoveFluid(int x, int y, int z) {
    const glm::ivec3 wp{x, y, z};
    FluidChunk* c = GetChunk(WorldToChunk(wp));
    if (!c) return;
    const glm::ivec3 lp = WorldToLocal(wp);
    FluidCell& cell = c->cells[lp.x][lp.y][lp.z];
    if (!cell.exists) return;
    cell = {};
    --c->cellCount;
    c->dirty = true;
}

void FluidGrid::Clear() {
    for (auto& [pos, chunk] : chunks_) {
        if (fglDeleteBuffers) {
            if (chunk.ebo) { fglDeleteBuffers(1, &chunk.ebo); chunk.ebo = 0; }
            if (chunk.vbo) { fglDeleteBuffers(1, &chunk.vbo); chunk.vbo = 0; }
        }
        if (fglDeleteVertexArrays && chunk.vao) {
            fglDeleteVertexArrays(1, &chunk.vao); chunk.vao = 0;
        }
    }
    chunks_.clear();
}

void FluidGrid::MarkAllDirty() {
    for (auto& [pos, chunk] : chunks_) chunk.dirty = true;
}

int FluidGrid::FluidCount() const {
    int total = 0;
    for (const auto& [pos, chunk] : chunks_) total += chunk.cellCount;
    return total;
}

struct IVec3Hash2 {
    size_t operator()(const glm::ivec3& v) const noexcept {
        size_t h = static_cast<size_t>(v.x) * 2654435761u;
        h ^= static_cast<size_t>(v.y) * 805459861u;
        h ^= static_cast<size_t>(v.z) * 3674653429u;
        return h;
    }
};

void FluidGrid::Tick(
    const std::function<bool(glm::ivec3)>&          hasBlockAt,
    const std::function<uint32_t(glm::ivec3)>&      getBlockID,
    const std::function<void(glm::ivec3, uint32_t)>& setBlock)
{
    if (!registry_) return;

    struct Node { glm::ivec3 pos; uint32_t fluidID; uint8_t level; };

    std::unordered_map<glm::ivec3, std::pair<uint32_t, uint8_t>, IVec3Hash2> newState;
    std::unordered_set<glm::ivec3, IVec3Hash2> sourceSet;

    std::queue<Node> queue;

    for (auto& [chunkPos, chunk] : chunks_) {
        for (int lx = 0; lx < kChunkSize; ++lx)
        for (int ly = 0; ly < kChunkSize; ++ly)
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const FluidCell& cell = chunk.cells[lx][ly][lz];
            if (!cell.exists || !cell.isSource) continue;
            const FluidData* fd = registry_->Get(cell.fluidID);
            if (!fd) continue;
            const glm::ivec3 wp = chunkPos * kChunkSize + glm::ivec3(lx, ly, lz);
            newState[wp]  = {cell.fluidID, static_cast<uint8_t>(fd->sourceLevel)};
            sourceSet.insert(wp);
            queue.push({wp, cell.fluidID, static_cast<uint8_t>(fd->sourceLevel)});
        }
    }

    constexpr glm::ivec3 kSideDirs[4] = {{1,0,0},{-1,0,0},{0,0,1},{0,0,-1}};

    while (!queue.empty()) {
        const Node n = queue.front(); queue.pop();

        const FluidData* fd = registry_->Get(n.fluidID);
        if (!fd) continue;

        const glm::ivec3 below = n.pos + glm::ivec3(0, -1, 0);
        if (!hasBlockAt(below)) {
            const uint8_t fallLevel = static_cast<uint8_t>(fd->sourceLevel - 1);
            auto it = newState.find(below);
            if (it == newState.end() || it->second.second < fallLevel) {
                newState[below] = {n.fluidID, fallLevel};
                queue.push({below, n.fluidID, fallLevel});
            }
        }

        if (n.level <= 1) continue;

        const uint8_t nextLevel = n.level - 1;
        for (const auto& dir : kSideDirs) {
            const glm::ivec3 np = n.pos + dir;
            if (hasBlockAt(np)) continue;
            auto it = newState.find(np);
            if (it != newState.end() && it->second.second >= nextLevel) continue;
            newState[np] = {n.fluidID, nextLevel};
            queue.push({np, n.fluidID, nextLevel});
        }
    }

    if (!newState.empty()) {
        for (auto& [pos, state] : newState) {
            if (sourceSet.count(pos)) continue;
            const FluidData* fd = registry_->Get(state.first);
            if (!fd || !fd->createsSources) continue;

            int sourceNeighbours = 0;
            for (const auto& dir : kSideDirs) {
                const glm::ivec3 np = pos + dir;
                if (sourceSet.count(np)) {
                    const auto it = newState.find(np);
                    if (it != newState.end() && it->second.first == state.first)
                        ++sourceNeighbours;
                }
            }
            if (sourceNeighbours >= 2) {
                state.second = static_cast<uint8_t>(fd->sourceLevel);
                sourceSet.insert(pos);
            }
        }
    }

    std::vector<glm::ivec3> interactionRemovals;
    constexpr glm::ivec3 kAllDirs[6] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

    for (const auto& [pos, state] : newState) {
        const FluidData* fd = registry_->Get(state.first);
        if (!fd || fd->interactions.empty()) continue;

        for (const auto& entry : fd->interactions) {
            if (entry.targetType != FluidInteractionEntry::TargetType::Block) continue;

            for (int di = 0; di < 6; ++di) {
                bool dirMatch = false;
                switch (entry.direction) {
                    case FluidInteractionDirection::Any:    dirMatch = true; break;
                    case FluidInteractionDirection::Sides:  dirMatch = (di == 0||di==1||di==4||di==5); break;
                    case FluidInteractionDirection::Top:    dirMatch = (di == 2); break;
                    case FluidInteractionDirection::Bottom: dirMatch = (di == 3); break;
                    case FluidInteractionDirection::None:   dirMatch = false; break;
                }
                if (!dirMatch) continue;

                const glm::ivec3 neighbour = pos + kAllDirs[di];
                const uint32_t nBlockID = getBlockID(neighbour);
                if (nBlockID == 0) continue;

                //TODO: wire to BlockRegistry name lookup for full accuracy.
                if (!entry.changeTo.empty()) {
                    interactionRemovals.push_back(pos);
                }
            }
        }
    }

    for (const auto& pos : interactionRemovals) {
        newState.erase(pos);
    }

    std::vector<glm::ivec3> toRemove;
    for (const auto& [chunkPos, chunk] : chunks_) {
        for (int lx = 0; lx < kChunkSize; ++lx)
        for (int ly = 0; ly < kChunkSize; ++ly)
        for (int lz = 0; lz < kChunkSize; ++lz) {
            const FluidCell& cell = chunk.cells[lx][ly][lz];
            if (!cell.exists || cell.isSource) continue;
            const glm::ivec3 wp = chunkPos * kChunkSize + glm::ivec3(lx, ly, lz);
            if (newState.find(wp) == newState.end()) {
                toRemove.push_back(wp);
            }
        }
    }
    for (const auto& pos : toRemove) {
        RemoveFluid(pos.x, pos.y, pos.z);
    }

    for (const auto& [pos, state] : newState) {
        const bool isSrc = (sourceSet.count(pos) > 0);
        SetFluid(pos.x, pos.y, pos.z, state.first, state.second, isSrc);
    }
}

namespace {
    static constexpr float kSourceHeight = 0.875f;

    float ComputeCornerHeight(int wx, int wy, int wz,
                               int dx, int dz,
                               uint32_t fluidID, int sourceLevel,
                               const FluidGrid& grid)
    {
        const int xs[4] = { 0, dx,  0, dx };
        const int zs[4] = { 0,  0, dz, dz };

        bool hasSource = false;
        float sum      = 0.0f;
        int   count    = 0;

        for (int i = 0; i < 4; ++i) {
            const int nx = wx + xs[i], nz = wz + zs[i];
            const uint32_t id  = grid.GetIDAtWorld   (nx, wy, nz);
            const uint8_t  lvl = grid.GetLevelAtWorld(nx, wy, nz);
            if (id != fluidID || lvl == 0) continue;
            if (lvl >= static_cast<uint8_t>(sourceLevel)) { hasSource = true; break; }
            sum += static_cast<float>(lvl) / static_cast<float>(sourceLevel);
            ++count;
        }

        if (hasSource) return kSourceHeight;
        if (count == 0) {
            const uint8_t lvl = grid.GetLevelAtWorld(wx, wy, wz);
            return static_cast<float>(lvl) / static_cast<float>(sourceLevel) * kSourceHeight;
        }
        return (sum / static_cast<float>(count)) * kSourceHeight;
    }

    void EmitQuad(std::vector<float>& verts, std::vector<uint32_t>& indices,
                  glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                  float u0, float v0uv, float u1, float v1uv,
                  float tileOriginU, float tileOriginV,
                  glm::vec3 normal)
    {
        const auto base = static_cast<uint32_t>(verts.size() / 10);

        const float uvs[4][2] = {
            {u0, v0uv}, {u1, v0uv}, {u1, v1uv}, {u0, v1uv}
        };
        const glm::vec3 positions[4] = {v0, v1, v2, v3};
        for (int i = 0; i < 4; ++i) {
            verts.push_back(positions[i].x);  verts.push_back(positions[i].y);  verts.push_back(positions[i].z);
            verts.push_back(uvs[i][0]);        verts.push_back(uvs[i][1]);
            verts.push_back(tileOriginU);      verts.push_back(tileOriginV);
            verts.push_back(normal.x);         verts.push_back(normal.y);        verts.push_back(normal.z);
        }
        indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
        indices.push_back(base + 2); indices.push_back(base + 3); indices.push_back(base + 0);
    }
}

void FluidGrid::RebuildChunkMesh(glm::ivec3 chunkPos, FluidChunk& chunk, const AtlasTexture& atlas) {
    std::vector<float>    verts;
    std::vector<uint32_t> indices;

    const int atlasW = atlas.Width()  > 0 ? atlas.Width()  : 1;
    const int atlasH = atlas.Height() > 0 ? atlas.Height() : 1;
    const float tileW = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(atlasW);
    const float tileH = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(atlasH);

    for (int lx = 0; lx < kChunkSize; ++lx)
    for (int ly = 0; ly < kChunkSize; ++ly)
    for (int lz = 0; lz < kChunkSize; ++lz) {
        const FluidCell& cell = chunk.cells[lx][ly][lz];
        if (!cell.exists) continue;

        const FluidData* fd = registry_->Get(cell.fluidID);
        if (!fd) continue;

        const glm::ivec3 wp = chunkPos * kChunkSize + glm::ivec3(lx, ly, lz);
        const int wx = wp.x, wy = wp.y, wz = wp.z;
        const uint32_t fid = cell.fluidID;
        const int sl  = fd->sourceLevel;

        const float bY = static_cast<float>(wy) - 0.5f;

        const bool fluidAbove = (GetIDAtWorld(wx, wy + 1, wz) == fid);

        //   vi=0: dx=-1, dz=+1  (front-left)
        //   vi=1: dx=+1, dz=+1  (front-right)
        //   vi=2: dx=+1, dz=-1  (back-right)
        //   vi=3: dx=-1, dz=-1  (back-left)
        float h[4];
        if (fluidAbove) {
            h[0] = h[1] = h[2] = h[3] = 1.0f;
        } else {
            h[0] = ComputeCornerHeight(wx, wy, wz, -1, +1, fid, sl, *this);
            h[1] = ComputeCornerHeight(wx, wy, wz, +1, +1, fid, sl, *this);
            h[2] = ComputeCornerHeight(wx, wy, wz, +1, -1, fid, sl, *this);
            h[3] = ComputeCornerHeight(wx, wy, wz, -1, -1, fid, sl, *this);
        }

        const float tY[4] = { bY + h[0], bY + h[1], bY + h[2], bY + h[3] };

        const float fx = static_cast<float>(wx);
        const float fz = static_cast<float>(wz);

        auto tileUV = [&](int faceIdx, float& ou, float& ov) {
            ou = static_cast<float>(fd->faceTiles[faceIdx].x) * tileW;
            ov = static_cast<float>(fd->faceTiles[faceIdx].y) * tileH;
        };

        if (!fluidAbove) {
            float ou, ov; tileUV(4, ou, ov);
            // vi=0: FL (x-0.5, tY[0], z+0.5)  vi=1: FR (x+0.5, tY[1], z+0.5)
            // vi=2: BR (x+0.5, tY[2], z-0.5)  vi=3: BL (x-0.5, tY[3], z-0.5)
            EmitQuad(verts, indices,
                {fx - 0.5f, tY[0], fz + 0.5f},
                {fx + 0.5f, tY[1], fz + 0.5f},
                {fx + 0.5f, tY[2], fz - 0.5f},
                {fx - 0.5f, tY[3], fz - 0.5f},
                ou, ov + tileH, ou + tileW, ov,
                ou, ov,
                {0.0f, 1.0f, 0.0f});
        }

        if (GetIDAtWorld(wx, wy - 1, wz) != fid) {
            float ou, ov; tileUV(5, ou, ov);
            EmitQuad(verts, indices,
                {fx - 0.5f, bY, fz - 0.5f},
                {fx + 0.5f, bY, fz - 0.5f},
                {fx + 0.5f, bY, fz + 0.5f},
                {fx - 0.5f, bY, fz + 0.5f},
                ou, ov + tileH, ou + tileW, ov,
                ou, ov,
                {0.0f, -1.0f, 0.0f});
        }

        if (GetIDAtWorld(wx, wy, wz + 1) != fid) {
            float ou, ov; tileUV(0, ou, ov);
            const float topL = tY[0], topR = tY[1];
            const float faceH = fluidAbove ? 1.0f : std::max(h[0], h[1]);
            EmitQuad(verts, indices,
                {fx - 0.5f, bY,   fz + 0.5f},
                {fx + 0.5f, bY,   fz + 0.5f},
                {fx + 0.5f, topR, fz + 0.5f},
                {fx - 0.5f, topL, fz + 0.5f},
                ou, ov + faceH * tileH, ou + tileW, ov,
                ou, ov,
                {0.0f, 0.0f, 1.0f});
        }

        if (GetIDAtWorld(wx, wy, wz - 1) != fid) {
            float ou, ov; tileUV(1, ou, ov);
            const float topL = tY[2], topR = tY[3];
            const float faceH = fluidAbove ? 1.0f : std::max(h[2], h[3]);
            EmitQuad(verts, indices,
                {fx + 0.5f, bY,   fz - 0.5f},
                {fx - 0.5f, bY,   fz - 0.5f},
                {fx - 0.5f, topR, fz - 0.5f},
                {fx + 0.5f, topL, fz - 0.5f},
                ou, ov + faceH * tileH, ou + tileW, ov,
                ou, ov,
                {0.0f, 0.0f, -1.0f});
        }

        if (GetIDAtWorld(wx - 1, wy, wz) != fid) {
            float ou, ov; tileUV(2, ou, ov);
            const float topF = tY[0], topB = tY[3];
            const float faceH = fluidAbove ? 1.0f : std::max(h[0], h[3]);
            EmitQuad(verts, indices,
                {fx - 0.5f, bY,   fz - 0.5f},
                {fx - 0.5f, bY,   fz + 0.5f},
                {fx - 0.5f, topF, fz + 0.5f},
                {fx - 0.5f, topB, fz - 0.5f},
                ou, ov + faceH * tileH, ou + tileW, ov,
                ou, ov,
                {-1.0f, 0.0f, 0.0f});
        }

        if (GetIDAtWorld(wx + 1, wy, wz) != fid) {
            float ou, ov; tileUV(3, ou, ov);
            const float topF = tY[1], topB = tY[2];
            const float faceH = fluidAbove ? 1.0f : std::max(h[1], h[2]);
            EmitQuad(verts, indices,
                {fx + 0.5f, bY,   fz + 0.5f},
                {fx + 0.5f, bY,   fz - 0.5f},
                {fx + 0.5f, topB, fz - 0.5f},
                {fx + 0.5f, topF, fz + 0.5f},
                ou, ov + faceH * tileH, ou + tileW, ov,
                ou, ov,
                {1.0f, 0.0f, 0.0f});
        }
    }

    if (verts.empty()) {
        chunk.indexCount = 0;
        return;
    }

    if (!EnsureChunkGPU(chunk)) return;

    fglBindVertexArray(chunk.vao);
    fglBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
    fglBufferData(GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                  verts.data(), GL_DYNAMIC_DRAW);

    fglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.ebo);
    fglBufferData(GL_ELEMENT_ARRAY_BUFFER,
                  static_cast<GLsizeiptr>(indices.size() * sizeof(uint32_t)),
                  indices.data(), GL_DYNAMIC_DRAW);

    chunk.indexCount = static_cast<int>(indices.size());
    fglBindVertexArray(0);
}

bool FluidGrid::EnsureChunkGPU(FluidChunk& chunk) {
    if (chunk.vao != 0) return true;
    if (!LoadGLFunctions()) return false;

    fglGenVertexArrays(1, &chunk.vao);
    fglGenBuffers(1, &chunk.vbo);
    fglGenBuffers(1, &chunk.ebo);

    fglBindVertexArray(chunk.vao);

    fglBindBuffer(GL_ARRAY_BUFFER, chunk.vbo);
    fglBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    fglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, chunk.ebo);
    fglBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    const GLsizei stride = 10 * static_cast<GLsizei>(sizeof(float));
    fglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    fglEnableVertexAttribArray(0);
    fglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    fglEnableVertexAttribArray(1);
    fglVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(5 * sizeof(float)));
    fglEnableVertexAttribArray(2);
    fglVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(7 * sizeof(float)));
    fglEnableVertexAttribArray(3);

    fglBindVertexArray(0);
    return true;
}

void FluidGrid::RebuildAll(const AtlasTexture& atlas) {
    for (auto& [pos, chunk] : chunks_) {
        if (chunk.cellCount == 0) { chunk.dirty = false; continue; }
        RebuildChunkMesh(pos, chunk, atlas);
        chunk.dirty = false;
    }
}

void FluidGrid::Draw(Shader& shader, const AtlasTexture& atlas,
                     const glm::mat4& projection, const glm::mat4& view,
                     const glm::mat4& lightSpaceMatrix)
{
    if (!LoadGLFunctions()) return;

    for (auto& [pos, chunk] : chunks_) {
        if (chunk.dirty) {
            if (chunk.cellCount > 0)
                RebuildChunkMesh(pos, chunk, atlas);
            else
                chunk.indexCount = 0;
            chunk.dirty = false;
        }
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);

    shader.Use();
    atlas.Bind(GL_TEXTURE0);
    shader.SetInt("uAtlas", 0);

    const glm::mat4 model(1.0f);
    shader.SetMat4("uModel", glm::value_ptr(model));
    shader.SetMat4("uMVP", glm::value_ptr(projection * view * model));
    shader.SetMat4("uLightSpaceMatrix", glm::value_ptr(lightSpaceMatrix));

    const int atlasW = atlas.Width()  > 0 ? atlas.Width()  : 1;
    const int atlasH = atlas.Height() > 0 ? atlas.Height() : 1;
    const float tileW = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(atlasW);
    const float tileH = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(atlasH);
    shader.SetVec2("uTileSize", tileW, tileH);

    for (auto& [pos, chunk] : chunks_) {
        if (chunk.indexCount == 0 || chunk.vao == 0) continue;
        fglBindVertexArray(chunk.vao);
        glDrawElements(GL_TRIANGLES, chunk.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    fglBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
