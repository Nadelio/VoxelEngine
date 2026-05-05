#pragma once

#include <string>
#include <vector>

#include "BiomeRegistry.hpp"
#include "BlockRegistry.hpp"
#include "Grid.hpp"

// A single layer in a Custom Superflat world, stacked bottom-to-top.
struct SuperflatLayer {
    uint32_t blockID   = 2; // default: stone
    int      thickness = 1; // number of blocks tall (1..255)
};

class TerrainGen {
public:
    struct Params {
        int seed = 0;

        // If non-empty, every column is forced into this biome (Single Biome world type).
        // Leave empty for automatic biome selection via temperature + elevation.
        std::string forceBiome;

        // Elevation algorithm (Noise Texture × Voronoi → Multiply → Square Root)
        // Scale controls feature frequency: feature size in blocks ≈ 1/scale.
        // e.g. 0.08 → hills ~12 blocks wide; 11.5 = sub-block noise.
        float noiseScale        = 0.08f;  // Blender Noise Texture: Scale (adapted)
        float voronoiScale      = 0.05f;  // Blender Voronoi Texture: Scale (adapted)
        float voronoiSmoothness = 0.633f; // Blender Voronoi Texture: Smoothness

        // Height mapping
        int baseHeight      = 0;  // minimum surface Y
        int heightAmplitude = 12; // maximum surface Y = baseHeight + heightAmplitude

        // World extent (centered at origin)
        int worldWidth = 64; // X: [-worldWidth/2, worldWidth/2)
        int worldDepth = 64; // Z: [-worldDepth/2, worldDepth/2)

        // Custom Superflat layers (bottom to top). When non-empty, all noise-based
        // generation is skipped and blocks are placed directly from these layers.
        std::vector<SuperflatLayer> superflatLayers;
    };

    // Fill grid with procedurally generated terrain.
    //
    // When `biomes` is non-null and contains entries, each column's biome is determined
    // by sampling a large-scale temperature noise map and calling BiomeRegistry::FindMatch.
    // When `params.forceBiome` is non-empty it overrides biome selection entirely (Single Biome).
    // Falls back to the legacy voronoi-based "plains"/"desert" split when `biomes` is null.
    static void Generate(Grid& grid, const BlockRegistry& registry,
                         const BiomeRegistry* biomes, const Params& params);

    // Returns the surface Y at world position (x, z) for the given params.
    static int SampleSurfaceY(float x, float z, const Params& p);

private:
    // sqrt(noise × voronoi) → [0, 1]
    static float SampleHeightFactor(float x, float z, const Params& p);

    // Large-scale noise remapped to [-1, 1] used as the temperature map.
    static float SampleTemperature(float x, float z, const Params& p);

    // voronoiColor → step(0.8) → multiply → smoothstep → [0, 1]  (legacy fallback)
    static float SampleBiomeFactor(float x, float z, const Params& p);

    // Single-octave value noise remapped to [0, 1].
    static float Noise2D(float x, float z, float scale, int seed);

    // Smooth F1 Voronoi — exponentially weighted blend of nearest-cell colors → [0, 1].
    static float Voronoi2D(float x, float z, float scale, float smoothness, int seed);

    // Returns the blockID from the registry best matching depth and biome.
    // Blocks matched via a group rule are chosen randomly (position-stable hash).
    // Individually-matched blocks use highest specificity score.
    // Falls back to fallbackID (typically stone) when nothing matches.
    static uint32_t SelectBlock(const BlockRegistry& registry,
                                int x, int y, int z,
                                int depth, const std::string& biome,
                                uint32_t fallbackID, int seed);
};
