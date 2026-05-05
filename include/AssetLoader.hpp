#pragma once

#include <array>
#include <cstdio>
#include <string>

#include "AtlasTexture.hpp"
#include "BiomeRegistry.hpp"
#include "BlockRegistry.hpp"
#include "DataFormat.hpp"
#include "TerrainGen.hpp"

// Parses a blocks.data file and registers all groups and block definitions
// into `registry`. Returns false on failure.
inline bool LoadBlocks(const std::string& path, const AtlasTexture* atlas, BlockRegistry& registry) {
	const auto doc = DataFormat::ParseFile(path);
	if(!doc) { return false; }

	static constexpr std::array<const char*, 6> kFaceNames = {
		"front", "back", "left", "right", "top", "bottom"
	};

	auto parseTerrain = [](const DataFormat::Object& obj) -> TerrainInfo {
		TerrainInfo t;
		if(const auto* v = obj.Get("temp"); v && v->IsFloatRange()) {
			t.temperatureMin = static_cast<float>(v->AsFloatRange().lo);
			t.temperatureMax = static_cast<float>(v->AsFloatRange().hi);
		}
		if(const auto* v = obj.Get("elevation"); v && v->IsIntRange()) {
			t.elevationMin = static_cast<int>(v->AsIntRange().lo);
			t.elevationMax = static_cast<int>(v->AsIntRange().hi);
		}
		if(const auto* v = obj.Get("depth"); v && v->IsIntRange()) {
			t.depthMin = static_cast<int>(v->AsIntRange().lo);
			t.depthMax = static_cast<int>(v->AsIntRange().hi);
		}
		if(const auto* v = obj.Get("biome")) {
			if(v->IsArray()) {
				for(const auto& elem : v->AsArray().elements) {
					if(elem && elem->IsTag()) t.biomes.push_back(elem->AsTag().name);
				}
			}
		} else {
			t.biomes.push_back("all");
		}
		return t;
	};

	for(const auto& [key, val] : doc->entries) {
		if(key != "group" || !val.IsObject()) { continue; }
		const DataFormat::Object& obj = val.AsObject();
		const DataFormat::Value* nameVal = obj.Get("name");
		if(!nameVal || !nameVal->IsString()) { continue; }
		BlockGroupData grp;
		grp.name    = nameVal->AsString();
		grp.terrain = parseTerrain(obj);
		registry.RegisterGroup(grp);
	}

	for(const auto& [key, val] : doc->entries) {
		if(key != "block" || !val.IsObject()) { continue; }
		const DataFormat::Object& obj = val.AsObject();

		const DataFormat::Value* idVal      = obj.Get("id");
		const DataFormat::Value* nameVal    = obj.Get("name");
		const DataFormat::Value* gravityVal = obj.Get("gravity");
		if(!idVal || !nameVal || !gravityVal) { continue; }
		if(!idVal->IsInt() || !nameVal->IsString() || !gravityVal->IsBool()) { continue; }

		const uint32_t    id      = static_cast<uint32_t>(idVal->AsInt());
		const std::string name    = nameVal->AsString();
		const bool        gravity = gravityVal->AsBool();

		FaceTileMap ftm{};
		bool valid = true;
		for(int i = 0; i < 6; ++i) {
			const DataFormat::Value* faceVal = obj.Get(kFaceNames[i]);
			if(!faceVal || !faceVal->IsArray()) { valid = false; break; }
			const DataFormat::TypedArray& arr = faceVal->AsArray();
			if(arr.elements.size() < 2) { valid = false; break; }
			const DataFormat::Value& fx = *arr.elements[0];
			const DataFormat::Value& fy = *arr.elements[1];
			if(!fx.IsInt() || !fy.IsInt()) { valid = false; break; }
			ftm[i] = FaceTile{static_cast<int>(fx.AsInt()), static_cast<int>(fy.AsInt())};
		}
		if(!valid) { continue; }

		BlockData blockDef;
		blockDef.blockID           = id;
		blockDef.name              = name;
		blockDef.faceTiles         = ftm;
		blockDef.atlas             = atlas;
		blockDef.affectedByGravity = gravity;

		if(const auto* v = obj.Get("group"); v && v->IsArray()) {
			for(const auto& elem : v->AsArray().elements) {
				if(elem && elem->IsTag()) blockDef.groups.push_back(elem->AsTag().name);
			}
		}

		blockDef.terrain = parseTerrain(obj);

		if(!registry.Register(blockDef)) {
			std::fprintf(stderr, "Block registration failed for ID %u (%s).\n", id, name.c_str());
			return false;
		}
	}
	return true;
}

// Parses a biomes.data file and populates `registry`. Returns false on failure or empty result.
inline bool LoadBiomes(const std::string& path, BiomeRegistry& registry) {
	const auto doc = DataFormat::ParseFile(path);
	if (!doc) return false;
	registry.Clear();
	for (const auto& [key, val] : doc->entries) {
		if (key != "biome" || !val.IsObject()) continue;
		const DataFormat::Object& obj = val.AsObject();
		BiomeData b;
		if (const auto* v = obj.Get("id");        v && v->IsString())      b.id            = v->AsString();
		if (const auto* v = obj.Get("name");      v && v->IsString())      b.displayName   = v->AsString();
		if (const auto* v = obj.Get("temp");      v && v->IsFloatRange()) {
			b.temperatureMin = static_cast<float>(v->AsFloatRange().lo);
			b.temperatureMax = static_cast<float>(v->AsFloatRange().hi);
		}
		if (const auto* v = obj.Get("elevation"); v && v->IsIntRange()) {
			b.elevationMin = static_cast<int>(v->AsIntRange().lo);
			b.elevationMax = static_cast<int>(v->AsIntRange().hi);
		}
		if (!b.id.empty()) registry.Register(b);
	}
	return !registry.Biomes().empty();
}
