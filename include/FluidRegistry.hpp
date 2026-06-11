#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "AtlasTexture.hpp"
#include "CubeMesh.hpp"

// Direction tag used in fluid interaction entries.
enum class FluidInteractionDirection { None, Bottom, Top, Sides, Any };

// A single interaction rule: when this fluid is adjacent to the target
// (fluid, block, or group name) from the given direction, the fluid cell
// is converted to `changeTo`.
struct FluidInteractionEntry {
    enum class TargetType { Fluid, Block, Group };
    TargetType                targetType = TargetType::Fluid;
    std::string               targetName;  // name/id of target
    FluidInteractionDirection direction   = FluidInteractionDirection::Any;
    std::string               changeTo;    // block or fluid name to replace with
};

// Shared data for a named fluid group.
struct FluidGroupData {
    std::string name;
};

// All properties of one fluid type loaded from fluids.data.
struct FluidData {
    uint32_t    fluidID       = 0;
    std::string name;
    int         sourceLevel   = 8;    // level of a permanent source block
    bool        createsSources = false; // two adjacent sources generate a new source between them

    std::vector<FluidInteractionEntry> interactions;

    FaceTileMap         faceTiles{};   // atlas tiles per face (same 6-face layout as blocks)
    const AtlasTexture* atlas = nullptr;

    // Terrain generation parameters
    float tempMin  = -1.0f, tempMax  =  1.0f;
    int   elevMin  = -255,  elevMax  =  256;
    bool  hasGenRules = false;

    std::vector<std::string> groups;
};

// Stores all registered fluid types and groups.
class FluidRegistry {
public:
    bool Register(const FluidData& fluid) {
        if (fluid.name.empty() || fluid.atlas == nullptr) return false;
        nameToID_[fluid.name] = fluid.fluidID;
        return fluids_.emplace(fluid.fluidID, fluid).second;
    }

    bool RegisterGroup(const FluidGroupData& grp) {
        if (grp.name.empty()) return false;
        return groups_.emplace(grp.name, grp).second;
    }

    void Clear() { fluids_.clear(); groups_.clear(); nameToID_.clear(); }

    const FluidData* Get(uint32_t id) const {
        auto it = fluids_.find(id);
        return it == fluids_.end() ? nullptr : &it->second;
    }

    const FluidData* GetByName(const std::string& name) const {
        auto it = nameToID_.find(name);
        if (it == nameToID_.end()) return nullptr;
        return Get(it->second);
    }

    uint32_t GetIDByName(const std::string& name) const {
        auto it = nameToID_.find(name);
        return it == nameToID_.end() ? 0 : it->second;
    }

    const FluidGroupData* GetGroup(const std::string& name) const {
        auto it = groups_.find(name);
        return it == groups_.end() ? nullptr : &it->second;
    }

    const std::unordered_map<uint32_t, FluidData>& Fluids() const { return fluids_; }

private:
    std::unordered_map<uint32_t, FluidData>       fluids_;
    std::unordered_map<std::string, FluidGroupData> groups_;
    std::unordered_map<std::string, uint32_t>     nameToID_;
};
