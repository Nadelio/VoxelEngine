#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "WorldFile.hpp"
#include "Compress.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {
    bool writeU8(std::FILE* f, uint8_t v) {
        return std::fwrite(&v, 1, 1, f) == 1;
    }

    bool writeU16(std::FILE* f, uint16_t v) {
        const uint8_t buf[2] = {
            static_cast<uint8_t>(v & 0xFF),
            static_cast<uint8_t>((v >> 8) & 0xFF),
        };
        return std::fwrite(buf, 1, 2, f) == 2;
    }

    bool writeI32(std::FILE* f, int32_t v) {
        const uint32_t u = static_cast<uint32_t>(v);
        const uint8_t buf[4] = {
            static_cast<uint8_t>(u        & 0xFF),
            static_cast<uint8_t>((u >>  8) & 0xFF),
            static_cast<uint8_t>((u >> 16) & 0xFF),
            static_cast<uint8_t>((u >> 24) & 0xFF),
        };
        return std::fwrite(buf, 1, 4, f) == 4;
    }

    bool writeU32(std::FILE* f, uint32_t v) {
        return writeI32(f, static_cast<int32_t>(v));
    }

    bool writeString(std::FILE* f, const std::string& s) {
        if (s.size() > 0xFFFF) return false;
        if (!writeU16(f, static_cast<uint16_t>(s.size()))) return false;
        if (!s.empty() && std::fwrite(s.data(), 1, s.size(), f) != s.size()) return false;
        return true;
    }

    bool readU8(std::FILE* f, uint8_t& out) {
        return std::fread(&out, 1, 1, f) == 1;
    }

    bool readU16(std::FILE* f, uint16_t& out) {
        uint8_t buf[2];
        if (std::fread(buf, 1, 2, f) != 2) return false;
        out = static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8);
        return true;
    }

    bool readI32(std::FILE* f, int32_t& out) {
        uint8_t buf[4];
        if (std::fread(buf, 1, 4, f) != 4) return false;
        const uint32_t u = static_cast<uint32_t>(buf[0])
                        | (static_cast<uint32_t>(buf[1]) <<  8)
                        | (static_cast<uint32_t>(buf[2]) << 16)
                        | (static_cast<uint32_t>(buf[3]) << 24);
        out = static_cast<int32_t>(u);
        return true;
    }

    bool readU32(std::FILE* f, uint32_t& out) {
        int32_t v;
        if (!readI32(f, v)) return false;
        out = static_cast<uint32_t>(v);
        return true;
    }

    bool readString(std::FILE* f, std::string& out) {
        uint16_t len;
        if (!readU16(f, len)) return false;
        out.resize(len);
        if (len > 0 && std::fread(out.data(), 1, len, f) != len) return false;
        return true;
    }

    bool writeI64(std::FILE* f, int64_t v) {
        const uint64_t u = static_cast<uint64_t>(v);
        const uint8_t buf[8] = {
            static_cast<uint8_t>(u        & 0xFF),
            static_cast<uint8_t>((u >>  8) & 0xFF),
            static_cast<uint8_t>((u >> 16) & 0xFF),
            static_cast<uint8_t>((u >> 24) & 0xFF),
            static_cast<uint8_t>((u >> 32) & 0xFF),
            static_cast<uint8_t>((u >> 40) & 0xFF),
            static_cast<uint8_t>((u >> 48) & 0xFF),
            static_cast<uint8_t>((u >> 56) & 0xFF),
        };
        return std::fwrite(buf, 1, 8, f) == 8;
    }

    bool readI64(std::FILE* f, int64_t& out) {
        uint8_t buf[8];
        if (std::fread(buf, 1, 8, f) != 8) return false;
        const uint64_t u = static_cast<uint64_t>(buf[0])
                        | (static_cast<uint64_t>(buf[1]) <<  8)
                        | (static_cast<uint64_t>(buf[2]) << 16)
                        | (static_cast<uint64_t>(buf[3]) << 24)
                        | (static_cast<uint64_t>(buf[4]) << 32)
                        | (static_cast<uint64_t>(buf[5]) << 40)
                        | (static_cast<uint64_t>(buf[6]) << 48)
                        | (static_cast<uint64_t>(buf[7]) << 56);
        out = static_cast<int64_t>(u);
        return true;
    }

    bool writeFloat(std::FILE* f, float v) {
        uint32_t u;
        std::memcpy(&u, &v, sizeof(u));
        return writeU32(f, u);
    }

    bool readFloat(std::FILE* f, float& out) {
        uint32_t u;
        if (!readU32(f, u)) return false;
        std::memcpy(&out, &u, sizeof(out));
        return true;
    }
}

bool WorldFile::Save(const std::string& path,
                     const Header& header,
                     const Grid& grid) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // header
    // magic
    if (std::fwrite("VXLW", 1, 4, f) != 4)          goto fail;
    // version 4
    if (!writeU8(f, 4))                               goto fail;
    // seed (int64)
    if (!writeI64(f, header.seed))                    goto fail;
    // world type
    if (!writeU8(f, static_cast<uint8_t>(header.worldType))) goto fail;
    // optional single-biome name
    if (header.worldType == WorldType::SingleBiome) {
        if (!writeString(f, header.singleBiome))      goto fail;
    }
    // optional superflat layers (bottom to top)
    if (header.worldType == WorldType::Superflat) {
        if (header.superflatLayers.size() > 0xFFFF)   goto fail;
        if (!writeU16(f, static_cast<uint16_t>(header.superflatLayers.size()))) goto fail;
        for (const auto& layer : header.superflatLayers) {
            if (!writeU32(f, layer.blockID))              goto fail;
            const int t = std::max(1, std::min(255, layer.thickness));
            if (!writeU8(f, static_cast<uint8_t>(t)))     goto fail;
        }
    }
    // datapack paths
    if (header.datapacks.size() > 0xFF)               goto fail;
    if (!writeU8(f, static_cast<uint8_t>(header.datapacks.size()))) goto fail;
    for (const auto& dp : header.datapacks) {
        if (!writeString(f, dp))                      goto fail;
    }
    // player position
    if (!writeFloat(f, header.playerPos.x))           goto fail;
    if (!writeFloat(f, header.playerPos.y))           goto fail;
    if (!writeFloat(f, header.playerPos.z))           goto fail;
    // end-of-header sentinel
    if (!writeU8(f, 0xFF))                            goto fail;

    // block records
    {
        std::vector<uint8_t> raw;
        grid.VisitBlocks([&](const glm::ivec3& pos, uint32_t blockID) {
            auto pushU32 = [&](uint32_t v) {
                raw.push_back(static_cast<uint8_t>(v        & 0xFF));
                raw.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
                raw.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
                raw.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
            };
            pushU32(blockID);
            pushU32(static_cast<uint32_t>(pos.x));
            pushU32(static_cast<uint32_t>(pos.y));
            pushU32(static_cast<uint32_t>(pos.z));
            raw.push_back(grid.GetBlockRotation(pos));
        });
        std::vector<uint8_t> payload;
        if (!Compress::Encode(raw.data(), raw.size(), payload)) goto fail;
        if (std::fwrite(payload.data(), 1, payload.size(), f) != payload.size()) goto fail;
    }

    std::fclose(f);
    return true;

fail:
    std::fclose(f);
    return false;
}

bool WorldFile::Load(const std::string& path,
                     Header& headerOut,
                     Grid& grid) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    Header h;

    // magic
    char magic[4];
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "VXLW", 4) != 0) goto fail;

    // version
    uint8_t version;
    if (!readU8(f, version) || version < 1 || version > 4) goto fail;

    // seed
    if (version == 1) {
        int32_t s32;
        if (!readI32(f, s32)) goto fail;
        h.seed = static_cast<int64_t>(s32);
    } else {
        if (!readI64(f, h.seed)) goto fail;
    }

    // world type
    {
        uint8_t wt;
        if (!readU8(f, wt)) goto fail;
        h.worldType = static_cast<WorldType>(wt);
    }

    // optional single-biome name
    if (h.worldType == WorldType::SingleBiome) {
        if (!readString(f, h.singleBiome)) goto fail;
    }
    // optional superflat layers
    if (h.worldType == WorldType::Superflat) {
        if (version == 1) {
            uint8_t layerCount;
            if (!readU8(f, layerCount)) goto fail;
            h.superflatLayers.resize(layerCount);
        } else {
            uint16_t layerCount;
            if (!readU16(f, layerCount)) goto fail;
            h.superflatLayers.resize(layerCount);
        }
        for (auto& layer : h.superflatLayers) {
            if (!readU32(f, layer.blockID)) goto fail;
            uint8_t thick;
            if (!readU8(f, thick)) goto fail;
            layer.thickness = static_cast<int>(thick);
        }
    }

    // datapack paths
    {
        uint8_t dpCount;
        if (!readU8(f, dpCount)) goto fail;
        h.datapacks.resize(dpCount);
        for (auto& dp : h.datapacks) {
            if (!readString(f, dp)) goto fail;
        }
    }

    // player position (v2 only)
    if (version >= 2) {
        if (!readFloat(f, h.playerPos.x)) goto fail;
        if (!readFloat(f, h.playerPos.y)) goto fail;
        if (!readFloat(f, h.playerPos.z)) goto fail;
        h.hasPlayerPos = true;
    }

    // end-of-header sentinel
    {
        uint8_t sentinel;
        if (!readU8(f, sentinel) || sentinel != 0xFF) goto fail;
    }

    // block records
    {
        struct BlockRecord { uint32_t id; int32_t x, y, z; uint8_t rotation = 0; };
        std::vector<BlockRecord> records;

        if (version == 4) {
            std::vector<uint8_t> fileBuf;
            {
                uint8_t chunk[4096];
                std::size_t n;
                while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0)
                    fileBuf.insert(fileBuf.end(), chunk, chunk + n);
            }
            std::vector<uint8_t> raw;
            if (!Compress::Decode(fileBuf.data(), fileBuf.size(), raw)) goto fail;
            std::size_t pos = 0;
            const std::size_t rawLen = raw.size();
            while (pos + 17 <= rawLen) {
                BlockRecord r;
                r.id = static_cast<uint32_t>(raw[pos])
                     | (static_cast<uint32_t>(raw[pos+1]) <<  8)
                     | (static_cast<uint32_t>(raw[pos+2]) << 16)
                     | (static_cast<uint32_t>(raw[pos+3]) << 24);
                pos += 4;
                r.x = static_cast<int32_t>(static_cast<uint32_t>(raw[pos])
                    | (static_cast<uint32_t>(raw[pos+1]) <<  8)
                    | (static_cast<uint32_t>(raw[pos+2]) << 16)
                    | (static_cast<uint32_t>(raw[pos+3]) << 24));
                pos += 4;
                r.y = static_cast<int32_t>(static_cast<uint32_t>(raw[pos])
                    | (static_cast<uint32_t>(raw[pos+1]) <<  8)
                    | (static_cast<uint32_t>(raw[pos+2]) << 16)
                    | (static_cast<uint32_t>(raw[pos+3]) << 24));
                pos += 4;
                r.z = static_cast<int32_t>(static_cast<uint32_t>(raw[pos])
                    | (static_cast<uint32_t>(raw[pos+1]) <<  8)
                    | (static_cast<uint32_t>(raw[pos+2]) << 16)
                    | (static_cast<uint32_t>(raw[pos+3]) << 24));
                pos += 4;
                r.rotation = raw[pos++];
                records.push_back(r);
            }
        } else {
            for (;;) {
                uint32_t id;
                if (!readU32(f, id)) {
                    if (std::feof(f)) break;
                    goto fail;
                }
                int32_t x, y, z;
                if (!readI32(f, x) || !readI32(f, y) || !readI32(f, z)) goto fail;
                uint8_t rot = 0;
                if (version >= 3) {
                    if (!readU8(f, rot)) goto fail;
                }
                records.push_back({id, x, y, z, rot});
            }
        }

        // clear and fill the grid.
        grid.Clear();
        for (const auto& r : records) {
            grid.AddBlockBulk(r.x, r.y, r.z, r.id, r.rotation);
        }
    }

    std::fclose(f);
    headerOut = h;
    return true;

fail:
    std::fclose(f);
    return false;
}

bool WorldFile::ReadHeader(const std::string& path, Header& headerOut) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    Header h;

    char magic[4];
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "VXLW", 4) != 0) goto fail;

    uint8_t version;
    if (!readU8(f, version) || version < 1 || version > 4) goto fail;

    // seed
    if (version == 1) {
        int32_t s32;
        if (!readI32(f, s32)) goto fail;
        h.seed = static_cast<int64_t>(s32);
    } else {
        if (!readI64(f, h.seed)) goto fail;
    }

    {
        uint8_t wt;
        if (!readU8(f, wt)) goto fail;
        h.worldType = static_cast<WorldType>(wt);
    }

    if (h.worldType == WorldType::SingleBiome) {
        if (!readString(f, h.singleBiome)) goto fail;
    }
    // optional superflat layers
    if (h.worldType == WorldType::Superflat) {
        if (version == 1) {
            uint8_t layerCount;
            if (!readU8(f, layerCount)) goto fail;
            h.superflatLayers.resize(layerCount);
        } else {
            uint16_t layerCount;
            if (!readU16(f, layerCount)) goto fail;
            h.superflatLayers.resize(layerCount);
        }
        for (auto& layer : h.superflatLayers) {
            if (!readU32(f, layer.blockID)) goto fail;
            uint8_t thick;
            if (!readU8(f, thick)) goto fail;
            layer.thickness = static_cast<int>(thick);
        }
    }

    {
        uint8_t dpCount;
        if (!readU8(f, dpCount)) goto fail;
        h.datapacks.resize(dpCount);
        for (auto& dp : h.datapacks) {
            if (!readString(f, dp)) goto fail;
        }
    }

    // player position (v2 only)
    if (version >= 2) {
        if (!readFloat(f, h.playerPos.x)) goto fail;
        if (!readFloat(f, h.playerPos.y)) goto fail;
        if (!readFloat(f, h.playerPos.z)) goto fail;
        h.hasPlayerPos = true;
    }

    {
        uint8_t sentinel;
        if (!readU8(f, sentinel) || sentinel != 0xFF) goto fail;
    }

    std::fclose(f);
    headerOut = h;
    return true;

fail:
    std::fclose(f);
    return false;
}
