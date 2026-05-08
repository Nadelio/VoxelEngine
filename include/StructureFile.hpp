#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Grid.hpp"

// Format (version 1):
// Header
//   [4]  magic        : "VXLS"
//   [1]  version      : uint8  (1=current)
//   [4]  originX      : int32_t  (little-endian)
//   [4]  originY      : int32_t
//   [4]  originZ      : int32_t
//   [4]  boundsX      : uint32_t  (width  in blocks)
//   [4]  boundsY      : uint32_t  (height in blocks)
//   [4]  boundsZ      : uint32_t  (depth  in blocks)
//   [1]  endOfHeader  : 0xFF sentinel
//
// Block records  (one per placed block, no explicit count)
//   [4]  blockID      : uint32_t
//   [4]  x            : int32_t  (relative to structure origin)
//   [4]  y            : int32_t
//   [4]  z            : int32_t
//
// All multi-byte integers are little-endian.
//
// Companion .data file (text, one per structure):
//   structure : {
//     frequency     : <float>,        # probability per eligible column  (0.0..1.0)
//     parent        : <tag|str>,      # name of a parent structure, or none for standalone
//     group         : <tag|str>,      # structure group (controls where it may generate)
//     rotations     : int[0,90,180,270], # allowed Y-axis rotations in degrees
//     required      : <bool>,         # must this piece appear at least once?
//     max_instances : <int>,          # max copies per generation pass  (-1 = unlimited)
//     variants      : string["a", "b"], # .struct file stems to pick from (omit = use def stem)
//     weights       : float[1.0, 2.0],  # relative spawn weights per variant (omit = uniform)
//     biomes        : tag[plains, forest], # biomes allowed to spawn in (omit = all biomes)
//     biome_multiple: float[1.0, 3.0],     # per-biome frequency multiplier, parallel to biomes (omit = uniform 1.0)
//   }
//
//   # named attachment points; repeated once per connector
//   connector : {
//     id     : <str|tag>,       # unique name within this structure
//     offset : int[x, y, z],   # local block offset from origin
//     facing : <tag>,           # direction the connector opens: north/south/east/west/up/down
//     type   : <tag|str>,       # connector type tag — two connectors match when types are equal
//   }
//
// Multi-part structures (dungeons, etc.):
//   Mark every piece with the same `group` tag.  Set `required : true` on the entry/start
//   piece.  Add `connector` entries to each piece.  The generator joins pieces by pairing
//   connectors whose `type` values match and whose `facing` directions are opposite.
//   Use `rotations` to allow a piece to be rotated before placing.

struct StructureFile {
    struct Header {
        glm::ivec3 origin = {0, 0, 0};  // local-space or world-space origin
        glm::uvec3 bounds = {0, 0, 0};  // (width, height, depth) in blocks
    };

    // A named attachment point where another structure piece may be joined.
    struct Connector {
        std::string id;              // unique name within the structure
        glm::ivec3  offset = {0,0,0}; // local position relative to structure origin
        std::string facing;          // "north","south","east","west","up","down"
        std::string type;            // connector type tag — matched against other pieces
    };

    // One selectable .struct file with a relative spawn weight.
    struct Variant {
        std::string name;           // .struct file stem (no extension)
        float       weight = 1.0f;  // relative weight; actual probability = weight / sum(weights)
    };

    // Generation rules loaded from the companion .data file.
    struct Def {
        float       frequency    = 0.0f;  // spawns-per-column probability [0.0, 1.0]
        std::string parent;               // parent structure name (empty = standalone)
        std::string group;                // structure group tag
        std::vector<int>       rotations; // allowed Y-axis rotations (multiples of 90°)
        bool        required     = false; // must appear at least once per group pass
        int         maxInstances = -1;    // -1 = unlimited
        std::vector<Variant>   variants;  // if empty, the def stem is the single variant (weight 1)
        std::vector<std::string> biomes;        // allowed biome ids; empty = all biomes
        std::vector<float> biomeMultiples;       // per-biome frequency multipliers; parallel to biomes
        std::vector<Connector> connectors;
    };

    // Save blocks from `grid` to a .struct file at `path`.
    static bool Save(const std::string& path,
                     const Header& header,
                     const Grid& grid);

    // Load a .struct file from `path`.
    // On success: populates `headerOut`, clears `grid`, and fills it with the saved blocks.
    // Returns false and leaves the grid unmodified on any error.
    static bool Load(const std::string& path,
                     Header& headerOut,
                     Grid& grid);

    // Read only the header from a .struct file, without loading block data.
    static bool ReadHeader(const std::string& path, Header& headerOut);

    // Parse the companion .data file at `path` (e.g. "structures/oak_tree.data").
    // Returns false if the file is missing or malformed.
    static bool LoadDef(const std::string& path, Def& defOut);

    // Scan `directory` for all *.data files and return (name, Def) pairs for each
    // that contains a valid "structure" entry.
    static std::vector<std::pair<std::string, Def>>
        ScanDefs(const std::string& directory);
};
