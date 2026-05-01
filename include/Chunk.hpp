#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include <glm/glm.hpp>
#include <SDL3/SDL_opengl.h>

#include "AtlasTexture.hpp"
#include "BlockRegistry.hpp"
#include "CubeMesh.hpp"

// A fixed 16×16×16 region of blocks that share a single combined GPU mesh.
class Chunk {
public:
    static constexpr int kSize = 16;

    Chunk() = default;
    ~Chunk();

    Chunk(const Chunk&) = delete;
    Chunk& operator=(const Chunk&) = delete;

    bool HasBlock(int lx, int ly, int lz) const;
    void SetBlock(int lx, int ly, int lz, uint32_t blockID);
    bool RemoveBlock(int lx, int ly, int lz);

    void MarkDirty() { dirty_ = true; }
    bool IsDirty() const { return dirty_; }

    // Rebuilds the combined vertex mesh.  hasBlockAtWorld is queried for neighbour
    // lookups that cross chunk boundaries.  chunkOrigin is this chunk's world origin.
    bool RebuildMesh(glm::ivec3 chunkOrigin, const AtlasTexture& atlas, const BlockRegistry& registry,
                     const std::function<bool(glm::ivec3)>& hasBlockAtWorld);

    void Draw() const;
    void DrawWireframe() const;

    // Iterate every set block: callback(localX, localY, localZ, blockID)
    template<typename Ft>
    void ForEachBlock(const Ft& callback) const{
        for (int lx = 0; lx < kSize; ++lx)
        for (int ly = 0; ly < kSize; ++ly)
        for (int lz = 0; lz < kSize; ++lz) {
            if (blocks_[lx][ly][lz].exists) {
                callback(lx, ly, lz, blocks_[lx][ly][lz].blockID);
            }
        }
    }

    bool IsEmpty() const { return blockCount_ == 0; }
    int BlockCount() const { return blockCount_; }

private:
    bool EnsureGPUResources();
    static bool LoadGLFunctions();

    struct CellData {
        bool exists = false;
        uint32_t blockID = 0;
    };

    CellData blocks_[kSize][kSize][kSize]{};
    int blockCount_ = 0;
    // Whether or not a chunk needs to be validated
    bool dirty_ = false;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;
    int indexCount_ = 0;
};
