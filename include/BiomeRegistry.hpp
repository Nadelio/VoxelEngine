#pragma once

#include <limits>
#include <string>
#include <vector>

// One biome as defined in biomes.data.
struct BiomeData {
    std::string id;                      // tag identifier used in blocks.data biome field
    std::string displayName;             // shown in the New World menu
    float       temperatureMin = -1.0f;
    float       temperatureMax =  1.0f;
    int         elevationMin   = -256;
    int         elevationMax   =  255;
};

// Registry of all loaded biomes.
class BiomeRegistry {
public:
    void Register(BiomeData b) { biomes_.push_back(std::move(b)); }
    void Clear() { biomes_.clear(); }

    const std::vector<BiomeData>& Biomes() const { return biomes_; }

    const BiomeData* GetById(const std::string& id) const {
        for (const auto& b : biomes_)
            if (b.id == id) return &b;
        return nullptr;
    }

    // Returns the best-matching biome for the given temperature and elevation.
    // "Best" = smallest temperature span that still contains both values.
    // Returns nullptr when nothing matches (caller should fall back to a default).
    const BiomeData* FindMatch(float temperature, int elevation) const {
        const BiomeData* best     = nullptr;
        float            bestSpan = std::numeric_limits<float>::max();
        for (const auto& b : biomes_) {
            if (temperature < b.temperatureMin || temperature > b.temperatureMax) continue;
            if (elevation   < b.elevationMin   || elevation   > b.elevationMax  ) continue;
            const float span = b.temperatureMax - b.temperatureMin;
            if (span < bestSpan) { bestSpan = span; best = &b; }
        }
        return best;
    }

private:
    std::vector<BiomeData> biomes_;
};
