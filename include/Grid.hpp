#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "AtlasTexture.hpp"
#include "BlockRegistry.hpp"
#include "Camera.hpp"
#include "Chunk.hpp"
#include "CubeMesh.hpp"
#include "Shader.hpp"

// A voxel grid backed by 16×16×16 chunks, each sharing one combined GPU mesh.
class Grid {
public:
    explicit Grid(const BlockRegistry* registry = nullptr) : registry_(registry) {}

    Grid(const Grid&) = delete;
    Grid& operator=(const Grid&) = delete;

    // Add a block at integer grid coordinates (x, y, z) using a registered block ID.
    // Returns false if a block already exists there.
    bool AddBlock(int x, int y, int z, uint32_t blockID);

    void SetRegistry(const BlockRegistry* registry) { registry_ = registry; }

    // Remove the block at the given grid position. Returns false if no block exists there.
    bool RemoveBlock(glm::ivec3 pos);

    // Remove all blocks and free their GPU resources.
    void Clear();

    // Draw all blocks; lazily rebuilds any dirty chunk meshes before rendering.
    void Draw(Shader& shader, const AtlasTexture& atlas,
              const glm::mat4& projection, const glm::mat4& view);

    // Returns true if a block occupies the given integer grid position.
    bool HasBlockAt(glm::ivec3 pos) const;

    // Mark all chunks dirty so they will be rebuilt on the next Draw() call.
    void RebuildVisibility();

    // Result of a camera ray-cast into the grid.
    struct LookedAtResult {
        bool hit = false;
        glm::ivec3 blockPos{};
        uint32_t blockID = 0;
        const BlockData* blockData = nullptr;
        // Face index: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
        int faceIndex = -1;
    };

    // Cast a ray from the camera and return info about the first block hit within maxDistance.
    LookedAtResult QueryLookedAt(const Camera& camera, float maxDistance = 8.0f) const;

    // Draw a blue wireframe outline around every block.
    void DrawWireframe(Shader& shader,
                       const glm::mat4& projection, const glm::mat4& view) const;

    // Draw a pink wireframe outline around the block currently looked at.
    void DrawLookedAtBlock(Shader& shader, const Camera& camera,
                           const glm::mat4& projection, const glm::mat4& view) const;

    // Draw a yellow quad outline on the specific face of the looked-at block.
    void DrawLookedAtFace(Shader& shader, const Camera& camera,
                          const glm::mat4& projection, const glm::mat4& view) const;

    int BlockCount() const;

    // Iterate all placed blocks in world space.
    void VisitBlocks(const std::function<void(const glm::ivec3&, uint32_t)>& callback) const;

    // Releases process-wide debug mesh GL resources created in Grid.cpp.
    static void ReleaseSharedGLResources();

    // A block at an arbitrary float position (used to render in-flight falling blocks).
    struct FloatBlock {
        glm::vec3 pos;
        uint32_t  blockID;
    };

    // Render a list of blocks at continuous float positions using the voxel shader.
    void DrawFloatBlocks(const std::vector<FloatBlock>& blocks,
                         Shader& shader, const AtlasTexture& atlas,
                         const glm::mat4& projection, const glm::mat4& view);

private:
    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const noexcept;
    };

    static int      FloorDiv(int a, int b);
    static glm::ivec3 ChunkCoord(glm::ivec3 worldPos);
    static glm::ivec3 LocalPos(glm::ivec3 worldPos, glm::ivec3 chunkCoord);

    template<typename Ft>
    void ForEachBlock(const Ft& callback) const{
        for (const auto& [coord, chunk] : chunks_) {
            const glm::ivec3 origin = coord * Chunk::kSize;
            chunk->ForEachBlock([&](int lx, int ly, int lz, uint32_t blockID) {
                callback(origin + glm::ivec3(lx, ly, lz), blockID);
            });
        }
    }
    void MarkNeighborChunksDirty(glm::ivec3 worldPos, glm::ivec3 chunkCoord, glm::ivec3 localPos);

    LookedAtResult FindLookedAt(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                                 float maxDistance) const;

    const BlockRegistry* registry_ = nullptr;
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> chunks_;
};
