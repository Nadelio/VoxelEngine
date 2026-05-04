#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "WorldFile.hpp"

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
}

bool WorldFile::Save(const std::string& path,
                     const Header& header,
                     const Grid& grid) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    // header
    // magic
    if (std::fwrite("VXLW", 1, 4, f) != 4)          goto fail;
    // version
    if (!writeU8(f, 1))                               goto fail;
    // seed
    if (!writeI32(f, header.seed))                    goto fail;
    // world type
    if (!writeU8(f, static_cast<uint8_t>(header.worldType))) goto fail;
    // optional single-biome name
    if (header.worldType == WorldType::SingleBiome) {
        if (!writeString(f, header.singleBiome))      goto fail;
    }
    // datapack paths
    if (header.datapacks.size() > 0xFF)               goto fail;
    if (!writeU8(f, static_cast<uint8_t>(header.datapacks.size()))) goto fail;
    for (const auto& dp : header.datapacks) {
        if (!writeString(f, dp))                      goto fail;
    }
    // end-of-header sentinel
    if (!writeU8(f, 0xFF))                            goto fail;

    // block records
    {
        bool ok = true;
        grid.VisitBlocks([&](const glm::ivec3& pos, uint32_t blockID) {
            if (!ok) return;
            if (!writeU32(f, blockID))     { ok = false; return; }
            if (!writeI32(f, pos.x))       { ok = false; return; }
            if (!writeI32(f, pos.y))       { ok = false; return; }
            if (!writeI32(f, pos.z))       { ok = false; return; }
        });
        if (!ok) goto fail;
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
    if (!readU8(f, version) || version != 1) goto fail;

    // seed
    if (!readI32(f, h.seed)) goto fail;

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

    // datapack paths
    {
        uint8_t dpCount;
        if (!readU8(f, dpCount)) goto fail;
        h.datapacks.resize(dpCount);
        for (auto& dp : h.datapacks) {
            if (!readString(f, dp)) goto fail;
        }
    }

    // end-of-header sentinel
    {
        uint8_t sentinel;
        if (!readU8(f, sentinel) || sentinel != 0xFF) goto fail;
    }

    // block records — load into a staging list so we only touch the grid on full success
    {
        struct BlockRecord { uint32_t id; int32_t x, y, z; };
        std::vector<BlockRecord> records;

        for (;;) {
            uint32_t id;
            if (!readU32(f, id)) {
                if (std::feof(f)) break; // clean end of file
                goto fail;
            }
            int32_t x, y, z;
            if (!readI32(f, x) || !readI32(f, y) || !readI32(f, z)) goto fail;
            records.push_back({id, x, y, z});
        }

        // clear and fill the grid.
        grid.Clear();
        for (const auto& r : records) {
            grid.AddBlock(r.x, r.y, r.z, r.id);
        }
    }

    std::fclose(f);
    headerOut = h;
    return true;

fail:
    std::fclose(f);
    return false;
}
