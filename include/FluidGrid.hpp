#pragma once

#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <SDL3/SDL_opengl.h>

#include "AtlasTexture.hpp"
#include "FluidRegistry.hpp"
#include "Shader.hpp"

// Stores fluid cells in a 16x16x16 chunked spatial structure.
// Handles Minecraft-style fluid simulation and mesh generation.
class FluidGrid {
public:
    static constexpr int kChunkSize = 16;

    explicit FluidGrid(const FluidRegistry* registry = nullptr) : registry_(registry) {}
    ~FluidGrid();

    FluidGrid(const FluidGrid&) = delete;
    FluidGrid& operator=(const FluidGrid&) = delete;

    void SetRegistry(const FluidRegistry* registry) { registry_ = registry; }

    // Place a fluid at world coordinates (marks chunk dirty).
    void SetFluid(int x, int y, int z, uint32_t fluidID, uint8_t level, bool isSource = false);

    // Bulk variant: does not mark dirty, caller must call MarkAllDirty() when done.
    void SetFluidBulk(int x, int y, int z, uint32_t fluidID, uint8_t level, bool isSource = false);

    void RemoveFluid(int x, int y, int z);

    bool     HasFluid   (int x, int y, int z) const;
    uint32_t GetFluidID (int x, int y, int z) const;
    uint8_t  GetFluidLevel(int x, int y, int z) const;

    void Clear();
    void MarkAllDirty();

    // Eagerly rebuild all chunk meshes (call after bulk placement).
    void RebuildAll(const AtlasTexture& atlas);

    int FluidCount() const;

    // Run one fluid simulation step.
    //   hasBlockAt  - returns true if a solid block occupies that world position
    //   getBlockID  - returns the block ID at that position (0 = none/air)
    //   setBlock    - called by interaction rules to place a block (replaces fluid)
    void Tick(const std::function<bool(glm::ivec3)>&          hasBlockAt,
              const std::function<uint32_t(glm::ivec3)>&      getBlockID,
              const std::function<void(glm::ivec3, uint32_t)>& setBlock);

    // Render all fluid chunks. Call with depth-write disabled and blending enabled.
    void Draw(Shader& shader, const AtlasTexture& atlas,
              const glm::mat4& projection, const glm::mat4& view,
              const glm::mat4& lightSpaceMatrix);

    // --- Internal accessors used by mesh generation helpers ---
    uint8_t  GetLevelAtWorld(int x, int y, int z) const;
    uint32_t GetIDAtWorld   (int x, int y, int z) const;

private:
    struct FluidCell {
        bool     exists   = false;
        uint32_t fluidID  = 0;
        uint8_t  level    = 0;      // 0=empty, 1..sourceLevel-1=flowing, sourceLevel=source
        bool     isSource = false;  // permanent source – never removed by simulation
    };

    struct FluidChunk {
        FluidCell cells[kChunkSize][kChunkSize][kChunkSize]{};
        int  cellCount  = 0;
        bool dirty      = true;
        GLuint vao      = 0;
        GLuint vbo      = 0;
        GLuint ebo      = 0;
        int  indexCount = 0;
    };

    struct IVec3Hash {
        size_t operator()(const glm::ivec3& v) const noexcept {
            size_t h = static_cast<size_t>(v.x) * 2654435761u;
            h ^= static_cast<size_t>(v.y) * 805459861u;
            h ^= static_cast<size_t>(v.z) * 3674653429u;
            return h;
        }
    };

    glm::ivec3  WorldToChunk(glm::ivec3 pos) const;
    glm::ivec3  WorldToLocal(glm::ivec3 pos) const;

    FluidChunk& GetOrCreateChunk(glm::ivec3 chunkPos);
    FluidChunk* GetChunk        (glm::ivec3 chunkPos);
    const FluidChunk* GetChunk  (glm::ivec3 chunkPos) const;

    void RebuildChunkMesh(glm::ivec3 chunkPos, FluidChunk& chunk, const AtlasTexture& atlas);
    bool EnsureChunkGPU  (FluidChunk& chunk);
    static bool LoadGLFunctions();

    std::unordered_map<glm::ivec3, FluidChunk, IVec3Hash> chunks_;
    const FluidRegistry* registry_ = nullptr;
};
