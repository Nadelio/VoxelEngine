#include "TerrainGen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "BiomeRegistry.hpp"

namespace {
    float hash2(int x, int z, int channel) {
        uint32_t h = static_cast<uint32_t>(x * 1619 + z * 31337 + channel * 6971);
        h ^= h >> 13;
        h *= 0xbf58476du;
        h ^= h >> 31;
        return static_cast<float>(h & 0x00FFFFFFu) * (1.0f / static_cast<float>(0x01000000u));
    }

    float smoothstep(float t) {
        return t * t * (3.0f - 2.0f * t);
    }
}

float TerrainGen::Noise2D(float x, float z, float scale, int seed) {
    x *= scale;
    z *= scale;
    const int   ix = static_cast<int>(std::floor(x));
    const int   iz = static_cast<int>(std::floor(z));
    const float fx = x - static_cast<float>(ix);
    const float fz = z - static_cast<float>(iz);

    const float sx = smoothstep(fx);
    const float sz = smoothstep(fz);

    const float v00 = hash2(ix,     iz,     seed);
    const float v10 = hash2(ix + 1, iz,     seed);
    const float v01 = hash2(ix,     iz + 1, seed);
    const float v11 = hash2(ix + 1, iz + 1, seed);

    return v00 * (1 - sx) * (1 - sz)
         + v10 *      sx  * (1 - sz)
         + v01 * (1 - sx) *      sz
         + v11 *      sx  *      sz;
}

float TerrainGen::Voronoi2D(float x, float z, float scale, float smoothness, int seed) {
    x *= scale;
    z *= scale;
    const int   ix = static_cast<int>(std::floor(x));
    const int   iz = static_cast<int>(std::floor(z));
    const float k  = smoothness * 4.0f + 0.001f;

    float totalW = 0.0f;
    float colorW = 0.0f;

    for (int dz = -2; dz <= 2; ++dz) {
        for (int dx = -2; dx <= 2; ++dx) {
            const int   cx    = ix + dx;
            const int   cz    = iz + dz;
            const float fpx   = static_cast<float>(cx) + hash2(cx, cz, seed);
            const float fpz   = static_cast<float>(cz) + hash2(cx, cz, seed + 1);
            const float color = hash2(cx, cz, seed + 2);
            const float dist  = std::sqrt((x - fpx) * (x - fpx) + (z - fpz) * (z - fpz));
            const float w     = std::exp(-dist / k);
            totalW += w;
            colorW += w * color;
        }
    }

    return colorW / (totalW + 1e-10f);
}

float TerrainGen::SampleHeightFactor(float x, float z, const Params& p) {
    const float noise   = Noise2D(x, z, p.noiseScale,   p.seed);
    const float voronoi = Voronoi2D(x, z, p.voronoiScale, p.voronoiSmoothness, p.seed);
    return std::sqrt(noise * voronoi);
}

float TerrainGen::SampleTemperature(float x, float z, const Params& p) {
    const float n = Noise2D(x, z, 0.008f, p.seed + 9999);
    return n * 2.0f - 1.0f;
}

float TerrainGen::SampleBiomeFactor(float x, float z, const Params& p) {
    const float v   = Voronoi2D(x, z, p.voronoiScale, p.voronoiSmoothness, p.seed);
    const float cr1 = (v >= 0.8f) ? 1.0f : 0.0f;
    const float raw = v * cr1;
    return smoothstep(raw);
}

int TerrainGen::SampleSurfaceY(float x, float z, const Params& p) {
    return p.baseHeight + static_cast<int>(SampleHeightFactor(x, z, p)
                                           * static_cast<float>(p.heightAmplitude));
}

uint32_t TerrainGen::SelectBlock(const BlockRegistry& registry,
                                  int x, int y, int z,
                                  int depth, const std::string& biome,
                                  uint32_t fallbackID, int seed) {
    auto terrainMatches = [&](const TerrainInfo& t) -> bool {
        if (depth < t.depthMin || depth > t.depthMax) return false;
        if (t.biomes.empty()) return true;
        if (std::find(t.biomes.begin(), t.biomes.end(), "all") != t.biomes.end()) return true;
        return std::find(t.biomes.begin(), t.biomes.end(), biome) != t.biomes.end();
    };

    auto terrainScore = [](const TerrainInfo& t) -> int {
        const bool isAny = t.biomes.empty()
            || std::find(t.biomes.begin(), t.biomes.end(), "all") != t.biomes.end();
        return (!isAny ? 10 : 0) + (255 - (t.depthMax - t.depthMin));
    };

    struct Candidate { const BlockData* bd; int score; bool fromGroup; };
    std::vector<Candidate> candidates;

    for (const auto& [id, bd] : registry.Blocks()) {
        bool matchedGroup = false;
        for (const auto& groupName : bd.groups) {
            const BlockGroupData* grp = registry.GetGroup(groupName);
            if (grp && terrainMatches(grp->terrain)) {
                candidates.push_back({&bd, terrainScore(grp->terrain), true});
                matchedGroup = true;
                break;
            }
        }
        if (matchedGroup) continue;

        if (!terrainMatches(bd.terrain)) continue;
        candidates.push_back({&bd, terrainScore(bd.terrain), false});
    }

    if (candidates.empty()) return fallbackID;

    int bestScore = -1;
    for (const auto& c : candidates) bestScore = std::max(bestScore, c.score);

    std::vector<const BlockData*> topGroup;
    const BlockData*              topIndividual = nullptr;
    for (const auto& c : candidates) {
        if (c.score < bestScore) continue;
        if (c.fromGroup) topGroup.push_back(c.bd);
        else             topIndividual = c.bd;
    }

    if (!topGroup.empty()) {
        uint32_t h = static_cast<uint32_t>(x * 1619 + y * 31337 + z * 6971 + seed * 1013);
        h ^= h >> 13; h *= 0xbf58476du; h ^= h >> 31;
        return topGroup[static_cast<std::size_t>(h) % topGroup.size()]->blockID;
    }

    return topIndividual ? topIndividual->blockID : fallbackID;
}

void TerrainGen::Generate(Grid& grid, const BlockRegistry& registry, const BiomeRegistry* biomes, const Params& p) {
    const int halfW = p.worldWidth  / 2;
    const int halfD = p.worldDepth  / 2;
    const int minY  = p.baseHeight - 4;

    for (int z = -halfD; z < halfD; ++z) {
        for (int x = -halfW; x < halfW; ++x) {
            const float fx = static_cast<float>(x);
            const float fz = static_cast<float>(z);

            const int surfaceY = SampleSurfaceY(fx, fz, p);

            std::string biome;
            if (!p.forceBiome.empty()) {
                biome = p.forceBiome;
            } else if (biomes && !biomes->Biomes().empty()) {
                const float temperature = SampleTemperature(fx, fz, p);
                const BiomeData* bd = biomes->FindMatch(temperature, surfaceY);
                biome = bd ? bd->id : biomes->Biomes().front().id;
            } else {
                const float biomeFactor = SampleBiomeFactor(fx, fz, p);
                biome = (biomeFactor > 0.0f) ? "desert" : "plains";
            }

            for (int y = minY; y <= surfaceY; ++y) {
                const int      depth   = surfaceY - y;
                const uint32_t blockID = SelectBlock(registry, x, y, z, depth, biome, 2u /* stone */, p.seed);
                grid.AddBlock(x, y, z, blockID);
            }
        }
    }
}
