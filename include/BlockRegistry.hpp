#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "AtlasTexture.hpp"
#include "CubeMesh.hpp"

// Describes where a block may appear during terrain generation.
struct TerrainInfo {
    float temperatureMin = -1.0f;
    float temperatureMax =  1.0f;
    int   elevationMin   = -256;
    int   elevationMax   =  255;
    int   depthMin       =    0; // 0 = surface
    int   depthMax       =  255;
    std::vector<std::string> biomes; // empty = any biome
};

// Shared terrain generation rules for a named group of blocks.
// Blocks belonging to a group inherit these rules, their own terrain field acts as a fallback.
struct BlockGroupData {
    std::string name;
    TerrainInfo terrain{};
};

struct BlockData {
    uint32_t blockID = 0;
    std::string name;
    FaceTileMap faceTiles{};
    const AtlasTexture* atlas = nullptr;
    bool affectedByGravity = false;
    std::vector<std::string> groups;
    TerrainInfo terrain{};
};

class BlockRegistry {
public:
    bool Register(const BlockData& block) {
        if (block.name.empty() || block.atlas == nullptr) {
            return false;
        }
        return blocks_.emplace(block.blockID, block).second;
    }

    bool RegisterGroup(const BlockGroupData& group) {
        if (group.name.empty()) return false;
        return groups_.emplace(group.name, group).second;
    }

    void Clear() { blocks_.clear(); groups_.clear(); }

    const BlockData* Get(uint32_t blockID) const {
        const auto it = blocks_.find(blockID);
        if (it == blocks_.end()) {
            return nullptr;
        }
        return &it->second;
    }

    const BlockGroupData* GetGroup(const std::string& name) const {
        const auto it = groups_.find(name);
        return it == groups_.end() ? nullptr : &it->second;
    }

    const std::unordered_map<uint32_t, BlockData>& Blocks() const { return blocks_; }

private:
    std::unordered_map<uint32_t, BlockData> blocks_;
    std::unordered_map<std::string, BlockGroupData> groups_;
};
