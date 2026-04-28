#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include "AtlasTexture.hpp"
#include "CubeMesh.hpp"

struct BlockData {
    uint32_t blockID = 0;
    std::string name;
    FaceTileMap faceTiles{};
    const AtlasTexture* atlas = nullptr;
    bool affectedByGravity = false;
};

class BlockRegistry {
public:
    bool Register(const BlockData& block) {
        if (block.name.empty() || block.atlas == nullptr) {
            return false;
        }
        return blocks_.emplace(block.blockID, block).second;
    }

    const BlockData* Get(uint32_t blockID) const {
        const auto it = blocks_.find(blockID);
        if (it == blocks_.end()) {
            return nullptr;
        }
        return &it->second;
    }

private:
    std::unordered_map<uint32_t, BlockData> blocks_;
};
