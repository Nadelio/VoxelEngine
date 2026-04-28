#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include "AtlasTexture.hpp"
#include "Camera.hpp"
#include "Chunk.hpp"
#include "CubeMesh.hpp"
#include "Shader.hpp"

// A sparse voxel grid backed by 16×16×16 chunks, each sharing one combined GPU mesh.
class Grid {
public:
    Grid() = default;

    Grid(const Grid&) = delete;
    Grid& operator=(const Grid&) = delete;

    // Add a block at integer grid coordinates (x, y, z) with per-face atlas tile configuration.
    // Returns false if a block already exists there.
    bool AddBlock(int x, int y, int z, const AtlasTexture& atlas, const FaceTileMap& faceTiles);

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
        // Face index: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
        int faceIndex = -1;
        FaceTileMap faceTiles{};
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

private:
    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const noexcept;
    };

    static int      FloorDiv(int a, int b);
    static glm::ivec3 ChunkCoord(glm::ivec3 worldPos);
    static glm::ivec3 LocalPos(glm::ivec3 worldPos, glm::ivec3 chunkCoord);

    void ForEachBlock(const std::function<void(const glm::ivec3&, const FaceTileMap&)>& callback) const;
    void MarkNeighborChunksDirty(glm::ivec3 worldPos, glm::ivec3 chunkCoord, glm::ivec3 localPos);

    LookedAtResult FindLookedAt(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                                 float maxDistance) const;

    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> chunks_;
};
