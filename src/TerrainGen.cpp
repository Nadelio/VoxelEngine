#include "TerrainGen.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "BiomeRegistry.hpp"
#include "StructureFile.hpp"

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

float TerrainGen::Noise3D(float x, float y, float z, float scale, int seed) {
    x *= scale;
    y *= scale;
    z *= scale;
    const int   ix = static_cast<int>(std::floor(x));
    const int   iy = static_cast<int>(std::floor(y));
    const int   iz = static_cast<int>(std::floor(z));
    const float fx = x - static_cast<float>(ix);
    const float fy = y - static_cast<float>(iy);
    const float fz = z - static_cast<float>(iz);
    const float sx = smoothstep(fx);
    const float sy = smoothstep(fy);
    const float sz = smoothstep(fz);

    auto corner = [seed](int cx, int cy, int cz) -> float {
        uint32_t h = static_cast<uint32_t>(cx * 1619 + cy * 9473 + cz * 31337 + seed * 6971);
        h ^= h >> 13;
        h *= 0xbf58476du;
        h ^= h >> 31;
        return static_cast<float>(h & 0x00FFFFFFu) * (1.0f / static_cast<float>(0x01000000u));
    };

    const float v000 = corner(ix,   iy,   iz  );
    const float v100 = corner(ix+1, iy,   iz  );
    const float v010 = corner(ix,   iy+1, iz  );
    const float v110 = corner(ix+1, iy+1, iz  );
    const float v001 = corner(ix,   iy,   iz+1);
    const float v101 = corner(ix+1, iy,   iz+1);
    const float v011 = corner(ix,   iy+1, iz+1);
    const float v111 = corner(ix+1, iy+1, iz+1);

    const float a0 = v000*(1.0f-sx) + v100*sx;
    const float a1 = v010*(1.0f-sx) + v110*sx;
    const float a2 = v001*(1.0f-sx) + v101*sx;
    const float a3 = v011*(1.0f-sx) + v111*sx;
    const float b0 = a0*(1.0f-sy)   + a1*sy;
    const float b1 = a2*(1.0f-sy)   + a3*sy;
    return b0*(1.0f-sz) + b1*sz;
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
    const float noise   = Noise2D(x, z, p.noiseScale,   static_cast<int>(p.seed));
    const float voronoi = Voronoi2D(x, z, p.voronoiScale, p.voronoiSmoothness, static_cast<int>(p.seed));
    return std::sqrt(noise * voronoi);
}

float TerrainGen::SampleTemperature(float x, float z, const Params& p) {
    const float n = Noise2D(x, z, 0.008f, static_cast<int>(p.seed + 9999));
    return n * 2.0f - 1.0f;
}

float TerrainGen::SampleBiomeFactor(float x, float z, const Params& p) {
    const float v   = Voronoi2D(x, z, p.voronoiScale, p.voronoiSmoothness, static_cast<int>(p.seed));
    const float cr1 = (v >= 0.8f) ? 1.0f : 0.0f;
    const float raw = v * cr1;
    return smoothstep(raw);
}

int TerrainGen::SampleSurfaceY(float x, float z, const Params& p) {
    return p.baseHeight + static_cast<int>(SampleHeightFactor(x, z, p)
                                           * static_cast<float>(p.heightAmplitude));
}

std::string TerrainGen::GetBiomeAt(float x, float z, const BiomeRegistry* biomes, const Params& p) {
    if (!p.superflatLayers.empty())
        return {};
    if (!p.forceBiome.empty())
        return p.forceBiome;
    if (biomes && !biomes->Biomes().empty()) {
        const float temperature = SampleTemperature(x, z, p);
        const int   surfaceY    = SampleSurfaceY(x, z, p);
        const BiomeData* bd     = biomes->FindMatch(temperature, surfaceY);
        return bd ? bd->id : biomes->Biomes().front().id;
    }
    const float biomeFactor = SampleBiomeFactor(x, z, p);
    return (biomeFactor > 0.0f) ? "desert" : "plains";
}

uint32_t TerrainGen::SelectBlock(const BlockRegistry& registry,
                                  int x, int y, int z,
                                  int depth, const std::string& biome,
                                  uint32_t fallbackID, int seed) {
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const float fz = static_cast<float>(z);
    const int surfaceY = y + depth;

    auto passesSpatialTest = [&](GenTag tag, uint32_t blockID) -> bool {
        switch (tag) {
            case GenTag::Blob:
                return Noise3D(fx, fy, fz, 0.15f, seed + 12345 + static_cast<int>(blockID) * 7919) > 0.68f;
            case GenTag::Vein:
                return Noise3D(fx * 1.2f, fy * 2.0f, fz * 1.2f, 0.09f, seed + 54321 + static_cast<int>(blockID) * 6553) > 0.78f;
            case GenTag::Mix:
            default:
                return true;
        }
    };

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

    auto individualTerrainMatches = [&](const TerrainInfo& t) -> bool {
        if (t.hasElevationRule && (surfaceY < t.elevationMin || surfaceY > t.elevationMax)) return false;
        return terrainMatches(t);
    };

    struct Candidate { const BlockData* bd; int score; bool fromGroup; };
    std::vector<Candidate> candidates;

    for (const auto& [id, bd] : registry.Blocks()) {
        bool matchedGroup = false;
        for (const auto& groupName : bd.groups) {
            const BlockGroupData* grp = registry.GetGroup(groupName);
            if (grp && grp->terrain.hasGenRules && terrainMatches(grp->terrain)
                && (!bd.terrain.hasGenRules || individualTerrainMatches(bd.terrain))
                && passesSpatialTest(bd.hasCustomGenTag ? bd.genTag : grp->genTag, bd.blockID)) {
                const bool isSpecialized = bd.hasCustomGenTag && bd.genTag != GenTag::Mix;
                candidates.push_back({&bd, terrainScore(grp->terrain) + (isSpecialized ? 500 : 0), true});
                matchedGroup = true;
                break;
            }
        }
        if (matchedGroup) continue;

        if (!bd.terrain.hasGenRules) continue;
        if (!terrainMatches(bd.terrain)) continue;
        if (!passesSpatialTest(bd.genTag, bd.blockID)) continue;
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

void TerrainGen::PlaceStructures(Grid& grid, const BlockRegistry& /*registry*/, const BiomeRegistry* biomes, const Params& p) {
    if (p.structuresDir.empty()) return;

    auto defs = StructureFile::ScanDefs(p.structuresDir);
    if (defs.empty()) return;

    struct StructureData {
        StructureFile::Header                        header;
        std::vector<std::pair<glm::ivec3, uint32_t>> blocks;
    };
    std::unordered_map<std::string, StructureData> cache;

    auto loadStructure = [&](const std::string& stem) -> const StructureData* {
        auto it = cache.find(stem);
        if (it != cache.end()) return &it->second;

        const std::string path = p.structuresDir + "/" + stem + ".struct";
        Grid tempGrid;
        StructureFile::Header hdr;
        if (!StructureFile::Load(path, hdr, tempGrid)) return nullptr;

        StructureData sd;
        sd.header = hdr;
        tempGrid.VisitBlocks([&](const glm::ivec3& pos, uint32_t blockID) {
            sd.blocks.emplace_back(pos, blockID);
        });
        auto [ins, ok] = cache.emplace(stem, std::move(sd));
        return ok ? &ins->second : nullptr;
    };

    auto rotateY = [](glm::ivec3 off, int deg) -> glm::ivec3 {
        for (int r = 0, steps = deg / 90; r < steps; ++r) {
            const int nx =  off.z;
            const int nz = -off.x;
            off.x = nx;
            off.z = nz;
        }
        return off;
    };

    const int halfW = p.worldWidth  / 2;
    const int halfD = p.worldDepth  / 2;
    const int seed  = static_cast<int>(p.seed);

    for (std::size_t di = 0; di < defs.size(); ++di) {
        const auto& [defName, def] = defs[di];
        if (!def.parent.empty()) continue;
        if (def.frequency <= 0.0f) continue;

        int  instanceCount = 0;
        bool hitMax        = false;

        for (int z = -halfD; z < halfD && !hitMax; ++z) {
            for (int x = -halfW; x < halfW && !hitMax; ++x) {
                if (def.maxInstances >= 0 && instanceCount >= def.maxInstances) {
                    hitMax = true;
                    break;
                }

                float effectiveFreq = def.frequency;
                if (!def.biomes.empty()) {
                    const std::string colBiome = GetBiomeAt(
                        static_cast<float>(x), static_cast<float>(z), biomes, p);
                    std::size_t matchIdx = def.biomes.size();
                    for (std::size_t bi = 0; bi < def.biomes.size(); ++bi) {
                        if (def.biomes[bi] == colBiome || def.biomes[bi] == "all") {
                            matchIdx = bi;
                            break;
                        }
                    }
                    if (matchIdx == def.biomes.size()) continue;
                    if (matchIdx < def.biomeMultiples.size())
                        effectiveFreq *= def.biomeMultiples[matchIdx];
                }

                uint32_t h = static_cast<uint32_t>(
                    x * 1619 + z * 31337 + seed * 6971 + static_cast<int>(di) * 1013);
                h ^= h >> 13; h *= 0xbf58476du; h ^= h >> 31;
                const float roll = static_cast<float>(h & 0x00FFFFFFu)
                                 * (1.0f / static_cast<float>(0x01000000u));
                if (roll >= effectiveFreq) continue;

                const std::string* variantStem = nullptr;
                if (def.variants.empty()) {
                    variantStem = &defName;
                } else {
                    float totalW = 0.0f;
                    for (const auto& v : def.variants) totalW += v.weight;
                    uint32_t hv = static_cast<uint32_t>(
                        x * 2777 + z * 7919 + seed * 3343 + static_cast<int>(di) * 4423);
                    hv ^= hv >> 13; hv *= 0xbf58476du; hv ^= hv >> 31;
                    float pick = static_cast<float>(hv & 0x00FFFFFFu)
                               * (1.0f / static_cast<float>(0x01000000u)) * totalW;
                    for (const auto& v : def.variants) {
                        pick -= v.weight;
                        if (pick <= 0.0f) { variantStem = &v.name; break; }
                    }
                    if (!variantStem) variantStem = &def.variants.back().name;
                }
                if (!variantStem) continue;

                int rotation = 0;
                if (!def.rotations.empty()) {
                    uint32_t hr = static_cast<uint32_t>(
                        x * 3571 + z * 5039 + seed * 7919 + static_cast<int>(di) * 2311);
                    hr ^= hr >> 13; hr *= 0xbf58476du; hr ^= hr >> 31;
                    rotation = def.rotations[hr % def.rotations.size()];
                }

                const StructureData* sd = loadStructure(*variantStem);
                if (!sd) continue;

                const int surfaceY = SampleSurfaceY(
                    static_cast<float>(x), static_cast<float>(z), p);

                for (const auto& [localPos, blockID] : sd->blocks) {
                    const glm::ivec3 off      = rotateY(localPos - sd->header.origin, rotation);
                    const glm::ivec3 worldPos = { x + off.x, surfaceY + off.y, z + off.z };
                    grid.AddBlock(worldPos.x, worldPos.y, worldPos.z, blockID);
                }
                ++instanceCount;
            }
        }
    }
}

void TerrainGen::Generate(Grid& grid, const BlockRegistry& registry, const BiomeRegistry* biomes, const Params& p) {
    const int halfW = p.worldWidth  / 2;
    const int halfD = p.worldDepth  / 2;

    if (!p.superflatLayers.empty()) {
        int baseY = p.baseHeight;
        for (const auto& layer : p.superflatLayers) {
            for (int ly = 0; ly < layer.thickness; ++ly) {
                for (int z = -halfD; z < halfD; ++z) {
                    for (int x = -halfW; x < halfW; ++x) {
                        grid.AddBlockBulk(x, baseY + ly, z, layer.blockID);
                    }
                }
            }
            baseY += layer.thickness;
        }
        return;
    }

    const int minY  = p.minY;

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
                const uint32_t blockID = SelectBlock(registry, x, y, z, depth, biome, 2u /* stone */, static_cast<int>(p.seed));
                grid.AddBlockBulk(x, y, z, blockID);
            }
        }
    }

    grid.RebuildVisibility();
    PlaceStructures(grid, registry, biomes, p);
}

void TerrainGen::GenerateFluids(FluidGrid& fluidGrid, const Grid& blockGrid,
                                 const FluidRegistry& fluidRegistry, const Params& p) {
    if (p.superflatLayers.empty() == false) return;
    if (fluidRegistry.Fluids().empty()) return;

    const int halfW = p.worldWidth  / 2;
    const int halfD = p.worldDepth  / 2;
    const int seaLevel = p.baseHeight;

    for (const auto& [fid, fd] : fluidRegistry.Fluids()) {
        if (!fd.hasGenRules) {
            // if no rules, use the default sea-level fill
        }

        for (int z = -halfD; z < halfD; ++z)
        for (int x = -halfW; x < halfW; ++x) {
            if (fd.hasGenRules) {
                const float temp = SampleTemperature(static_cast<float>(x),
                                                     static_cast<float>(z), p);
                if (temp < fd.tempMin || temp > fd.tempMax) continue;
            }

            for (int y = seaLevel; y > seaLevel - fd.sourceLevel; --y) {
                if (!blockGrid.HasBlockAt({x, y, z}) && !fluidGrid.HasFluid(x, y, z)) {
                    const bool isSrc = (y == seaLevel);
                    fluidGrid.SetFluidBulk(x, y, z, fid,
                                           static_cast<uint8_t>(fd.sourceLevel), isSrc);
                }
            }
        }
    }

    fluidGrid.MarkAllDirty();
}
