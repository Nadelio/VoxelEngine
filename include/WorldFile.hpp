#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Grid.hpp"
#include "TerrainGen.hpp"

// Format:
// Header
//   [4]  magic          : "VXLW"
//   [1]  version        : uint8  (currently 1)
//   [4]  seed           : int32_t (little-endian)
//   [1]  worldType      : uint8  (0=default, 1=single_biome, 2=superflat)
//   # if worldType == 1 (single_biome)
//   [2]  biomeNameLen   : uint16_t
//   [N]  biomeName      : utf-8 bytes
//   # end if
//   [1]  datapackCount  : uint8
//   for each datapack:
//     [2]  pathLen      : uint16_t
//     [N]  path         : utf-8 bytes
//   [1]  endOfHeader    : 0xFF sentinel
//
// Block records  (one per placed block, no explicit count)
//   [4]  blockID        : uint32_t
//   [4]  x              : int32_t
//   [4]  y              : int32_t
//   [4]  z              : int32_t
//   # block instance data here
//
// All multi-byte integers are little-endian.

struct WorldFile {
    enum class WorldType : uint8_t {
        Default    = 0,
        SingleBiome = 1,
        Superflat  = 2,
    };

    // Metadata stored in the file header.
    struct Header {
        int32_t          seed         = 0;
        WorldType        worldType    = WorldType::Default;
        std::string      singleBiome; // only used when worldType == SingleBiome
        std::vector<std::string> datapacks;
    };

    // Save the current grid to `path`. Returns true on success.
    static bool Save(const std::string& path,
                     const Header& header,
                     const Grid& grid);

    // Load a world file from `path`.
    // On success: populates `headerOut`, clears `grid`, and fills it with the saved blocks.
    // Returns false and leaves the grid unmodified on any error.
    static bool Load(const std::string& path,
                     Header& headerOut,
                     Grid& grid);
};
