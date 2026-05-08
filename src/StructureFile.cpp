#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "StructureFile.hpp"
#include "Compress.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#include "DataFormat.hpp"

namespace {
    bool writeU8(std::FILE* f, uint8_t v) {
        return std::fwrite(&v, 1, 1, f) == 1;
    }

    bool writeU32(std::FILE* f, uint32_t v) {
        const uint8_t buf[4] = {
            static_cast<uint8_t>(v        & 0xFF),
            static_cast<uint8_t>((v >>  8) & 0xFF),
            static_cast<uint8_t>((v >> 16) & 0xFF),
            static_cast<uint8_t>((v >> 24) & 0xFF),
        };
        return std::fwrite(buf, 1, 4, f) == 4;
    }

    bool writeI32(std::FILE* f, int32_t v) {
        return writeU32(f, static_cast<uint32_t>(v));
    }

    bool readU8(std::FILE* f, uint8_t& out) {
        return std::fread(&out, 1, 1, f) == 1;
    }

    bool readU32(std::FILE* f, uint32_t& out) {
        uint8_t buf[4];
        if (std::fread(buf, 1, 4, f) != 4) return false;
        out = static_cast<uint32_t>(buf[0])
            | (static_cast<uint32_t>(buf[1]) <<  8)
            | (static_cast<uint32_t>(buf[2]) << 16)
            | (static_cast<uint32_t>(buf[3]) << 24);
        return true;
    }

    bool readI32(std::FILE* f, int32_t& out) {
        uint32_t u;
        if (!readU32(f, u)) return false;
        out = static_cast<int32_t>(u);
        return true;
    }
}

bool StructureFile::Save(const std::string& path,
                         const Header& header,
                         const Grid& grid) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;

    if (std::fwrite("VXLS", 1, 4, f) != 4)   goto fail;
    if (!writeU8 (f, 3))                      goto fail;
    if (!writeI32(f, header.origin.x))        goto fail;
    if (!writeI32(f, header.origin.y))        goto fail;
    if (!writeI32(f, header.origin.z))        goto fail;
    if (!writeU32(f, header.bounds.x))        goto fail;
    if (!writeU32(f, header.bounds.y))        goto fail;
    if (!writeU32(f, header.bounds.z))        goto fail;
    if (!writeU8 (f, 0xFF))                   goto fail;

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

bool StructureFile::Load(const std::string& path,
                         Header& headerOut,
                         Grid& grid) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    Header h;

    char magic[4];
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "VXLS", 4) != 0) goto fail;

    uint8_t version;
    if (!readU8(f, version) || version < 1 || version > 3) goto fail;

    if (!readI32(f, h.origin.x)) goto fail;
    if (!readI32(f, h.origin.y)) goto fail;
    if (!readI32(f, h.origin.z)) goto fail;
    if (!readU32(f, h.bounds.x)) goto fail;
    if (!readU32(f, h.bounds.y)) goto fail;
    if (!readU32(f, h.bounds.z)) goto fail;

    {
        uint8_t sentinel;
        if (!readU8(f, sentinel) || sentinel != 0xFF) goto fail;
    }

    {
        struct BlockRecord { uint32_t id; int32_t x, y, z; uint8_t rotation = 0; };
        std::vector<BlockRecord> records;

        if (version == 3) {
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
                if (version >= 2) {
                    if (!readU8(f, rot)) goto fail;
                }
                records.push_back({id, x, y, z, rot});
            }
        }

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

bool StructureFile::ReadHeader(const std::string& path, Header& headerOut) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;

    Header h;

    char magic[4];
    if (std::fread(magic, 1, 4, f) != 4 || std::memcmp(magic, "VXLS", 4) != 0) goto fail;

    {
        uint8_t version;
        if (!readU8(f, version) || version < 1 || version > 3) goto fail;
    }

    if (!readI32(f, h.origin.x)) goto fail;
    if (!readI32(f, h.origin.y)) goto fail;
    if (!readI32(f, h.origin.z)) goto fail;
    if (!readU32(f, h.bounds.x)) goto fail;
    if (!readU32(f, h.bounds.y)) goto fail;
    if (!readU32(f, h.bounds.z)) goto fail;

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

bool StructureFile::LoadDef(const std::string& path, Def& defOut) {
    auto doc = DataFormat::ParseFile(path);
    if (!doc) return false;

    bool foundStructure = false; 
    Def d;

    for (const auto& [key, val] : doc->entries) {
        if (key == "structure") {
            if (!val.IsObject()) return false;
            const auto& obj = val.AsObject();

            if (const auto* fv = obj.Get("frequency")) {
                if      (fv->IsFloat()) d.frequency = static_cast<float>(fv->AsFloat());
                else if (fv->IsInt())   d.frequency = static_cast<float>(fv->AsInt());
                else return false;
            }

            if (const auto* pv = obj.Get("parent")) {
                if      (pv->IsString()) d.parent = pv->AsString();
                else if (pv->IsTag())    d.parent = pv->AsTag().name;
                else return false;
                if (d.parent == "none") d.parent.clear();
            }

            if (const auto* gv = obj.Get("group")) {
                if      (gv->IsString()) d.group = gv->AsString();
                else if (gv->IsTag())    d.group = gv->AsTag().name;
                else return false;
            }

            if (const auto* rv = obj.Get("rotations")) {
                if (!rv->IsArray()) return false;
                const auto& arr = rv->AsArray();
                if (arr.elemType != DataFormat::TypedArray::ElemType::Int) return false;
                for (const auto& elem : arr.elements) {
                    if (!elem || !elem->IsInt()) return false;
                    const int rot = static_cast<int>(elem->AsInt());
                    if (rot != 0 && rot != 90 && rot != 180 && rot != 270) return false;
                    d.rotations.push_back(rot);
                }
            }

            if (const auto* bv = obj.Get("required")) {
                if (!bv->IsBool()) return false;
                d.required = bv->AsBool();
            }

            if (const auto* mv = obj.Get("max_instances")) {
                if (!mv->IsInt()) return false;
                d.maxInstances = static_cast<int>(mv->AsInt());
            }

            std::vector<std::string> variantNames;
            std::vector<float>       variantWeights;

            if (const auto* vv = obj.Get("variants")) {
                if (!vv->IsArray()) return false;
                const auto& arr = vv->AsArray();
                if (arr.elemType != DataFormat::TypedArray::ElemType::String) return false;
                for (const auto& elem : arr.elements) {
                    if (!elem || !elem->IsString()) return false;
                    variantNames.push_back(elem->AsString());
                }
            }

            if (const auto* wv = obj.Get("weights")) {
                if (!wv->IsArray()) return false;
                const auto& arr = wv->AsArray();
                if (arr.elemType != DataFormat::TypedArray::ElemType::Float &&
                    arr.elemType != DataFormat::TypedArray::ElemType::Int) return false;
                for (const auto& elem : arr.elements) {
                    if (!elem) return false;
                    float w = 0.0f;
                    if      (elem->IsFloat()) w = static_cast<float>(elem->AsFloat());
                    else if (elem->IsInt())   w = static_cast<float>(elem->AsInt());
                    else return false;
                    if (w < 0.0f) return false;
                    variantWeights.push_back(w);
                }
            }

            if (!variantWeights.empty() && variantWeights.size() != variantNames.size())
                return false;

            for (std::size_t i = 0; i < variantNames.size(); ++i) {
                Variant v;
                v.name   = variantNames[i];
                v.weight = variantWeights.empty() ? 1.0f : variantWeights[i];
                d.variants.push_back(std::move(v));
            }

            if (const auto* bv = obj.Get("biomes")) {
                if (!bv->IsArray()) return false;
                const auto& arr = bv->AsArray();
                if (arr.elemType != DataFormat::TypedArray::ElemType::Tag) return false;
                for (const auto& elem : arr.elements) {
                    if (!elem || !elem->IsTag()) return false;
                    d.biomes.push_back(elem->AsTag().name);
                }
            }

            if (const auto* mmv = obj.Get("biome_multiple")) {
                if (!mmv->IsArray()) return false;
                const auto& arr = mmv->AsArray();
                if (arr.elemType != DataFormat::TypedArray::ElemType::Float &&
                    arr.elemType != DataFormat::TypedArray::ElemType::Int) return false;
                for (const auto& elem : arr.elements) {
                    if (!elem) return false;
                    float m = 1.0f;
                    if      (elem->IsFloat()) m = static_cast<float>(elem->AsFloat());
                    else if (elem->IsInt())   m = static_cast<float>(elem->AsInt());
                    else return false;
                    d.biomeMultiples.push_back(m);
                }
                if (d.biomeMultiples.size() != d.biomes.size()) return false;
            }

            foundStructure = true;
            continue;
        }

        if (key == "connector") {
            if (!val.IsObject()) return false;
            const auto& obj = val.AsObject();

            Connector c;

            if (const auto* iv = obj.Get("id")) {
                if      (iv->IsString()) c.id = iv->AsString();
                else if (iv->IsTag())    c.id = iv->AsTag().name;
                else return false;
            } else {
                return false;
            }

            if (const auto* ov = obj.Get("offset")) {
                if (!ov->IsArray()) return false;
                const auto& arr = ov->AsArray();
                if (arr.elemType != DataFormat::TypedArray::ElemType::Int) return false;
                if (arr.elements.size() != 3) return false;
                for (int i = 0; i < 3; ++i) {
                    if (!arr.elements[i] || !arr.elements[i]->IsInt()) return false;
                }
                c.offset.x = static_cast<int>(arr.elements[0]->AsInt());
                c.offset.y = static_cast<int>(arr.elements[1]->AsInt());
                c.offset.z = static_cast<int>(arr.elements[2]->AsInt());
            } else {
                return false;
            }

            if (const auto* fv = obj.Get("facing")) {
                if      (fv->IsTag())    c.facing = fv->AsTag().name;
                else if (fv->IsString()) c.facing = fv->AsString();
                else return false;
            } else {
                return false;
            }

            if (const auto* tv = obj.Get("type")) {
                if      (tv->IsTag())    c.type = tv->AsTag().name;
                else if (tv->IsString()) c.type = tv->AsString();
                else return false;
            } else {
                return false;
            }

            d.connectors.push_back(std::move(c));
            continue;
        }
    }

    if (!foundStructure) return false;

    defOut = std::move(d);
    return true;
}

std::vector<std::pair<std::string, StructureFile::Def>>
StructureFile::ScanDefs(const std::string& directory) {
    std::vector<std::pair<std::string, Def>> results;

    namespace fs = std::filesystem;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(directory, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        const auto& p = entry.path();
        if (p.extension() != ".data") continue;

        Def d;
        if (LoadDef(p.string(), d))
            results.emplace_back(p.stem().string(), std::move(d));
    }

    return results;
}
