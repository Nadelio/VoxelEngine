#pragma once

#include <array>
#include <cstdio>
#include <string>

#include "AtlasTexture.hpp"
#include "BiomeRegistry.hpp"
#include "BlockRegistry.hpp"
#include "DataFormat.hpp"
#include "FluidRegistry.hpp"
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
			t.hasGenRules = true;
		}
		if(const auto* v = obj.Get("elevation"); v && v->IsIntRange()) {
			t.elevationMin = static_cast<int>(v->AsIntRange().lo);
			t.elevationMax = static_cast<int>(v->AsIntRange().hi);
			t.hasGenRules = true;
			t.hasElevationRule = true;
		}
		if(const auto* v = obj.Get("depth"); v && v->IsIntRange()) {
			t.depthMin = static_cast<int>(v->AsIntRange().lo);
			t.depthMax = static_cast<int>(v->AsIntRange().hi);
			t.hasGenRules = true;
		}
		if(const auto* v = obj.Get("biome")) {
			if(v->IsArray()) {
				for(const auto& elem : v->AsArray().elements) {
					if(elem && elem->IsTag()) t.biomes.push_back(elem->AsTag().name);
				}
			}
			t.hasGenRules = true;
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
		if(const auto* v = obj.Get("gen_tag"); v && v->IsTag()) {
			const std::string& tag = v->AsTag().name;
			if      (tag == "blob") grp.genTag = GenTag::Blob;
			else if (tag == "vein") grp.genTag = GenTag::Vein;
			else                    grp.genTag = GenTag::Mix;
		}
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

		if(const auto* v = obj.Get("gen_tag"); v && v->IsTag()) {
			const std::string& tag = v->AsTag().name;
			if      (tag == "blob") blockDef.genTag = GenTag::Blob;
			else if (tag == "vein") blockDef.genTag = GenTag::Vein;
			else                    blockDef.genTag = GenTag::Mix;
			blockDef.hasCustomGenTag = true;
		}

		if(const auto* v = obj.Get("can_rotate"); v && v->IsArray()) {
			for(const auto& elem : v->AsArray().elements) {
				if(!elem) continue;
				std::string tag;
				if(elem->IsTag()) tag = elem->AsTag().name;
				if(tag == "all")                      { blockDef.canRotate.x = blockDef.canRotate.y = blockDef.canRotate.z = true; break; }
				else if(tag == "none")                { break; }
				else if(tag == "X" || tag == "x")     blockDef.canRotate.x = true;
				else if(tag == "Y" || tag == "y")     blockDef.canRotate.y = true;
				else if(tag == "Z" || tag == "z")     blockDef.canRotate.z = true;
			}
		}

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

// Parses a fluids.data file and registers all groups and fluid definitions
// into `registry`. Returns false on failure.
inline bool LoadFluids(const std::string& path, const AtlasTexture* atlas, FluidRegistry& registry) {
	const auto doc = DataFormat::ParseFile(path);
	if (!doc) { return false; }

	static constexpr std::array<const char*, 6> kFaceNames = {
		"front", "back", "left", "right", "top", "bottom"
	};

	// Parse direction tag helper
	auto parseDirection = [](const std::string& tag) -> FluidInteractionDirection {
		if (tag == "bottom") return FluidInteractionDirection::Bottom;
		if (tag == "top")    return FluidInteractionDirection::Top;
		if (tag == "sides")  return FluidInteractionDirection::Sides;
		if (tag == "none")   return FluidInteractionDirection::None;
		return FluidInteractionDirection::Any; // "any" or unrecognised
	};

	// Register fluid groups first
	for (const auto& [key, val] : doc->entries) {
		if (key != "group" || !val.IsObject()) continue;
		const DataFormat::Object& obj = val.AsObject();
		const DataFormat::Value* nameVal = obj.Get("name");
		if (!nameVal || !nameVal->IsString()) continue;
		FluidGroupData grp;
		grp.name = nameVal->AsString();
		registry.RegisterGroup(grp);
	}

	// Register fluid definitions
	for (const auto& [key, val] : doc->entries) {
		if (key != "fluid" || !val.IsObject()) continue;
		const DataFormat::Object& obj = val.AsObject();

		const DataFormat::Value* idVal   = obj.Get("id");
		const DataFormat::Value* nameVal = obj.Get("name");
		if (!idVal || !nameVal || !idVal->IsInt() || !nameVal->IsString()) continue;

		FluidData fd;
		fd.fluidID = static_cast<uint32_t>(idVal->AsInt());
		fd.name    = nameVal->AsString();
		fd.atlas   = atlas;

		if (const auto* v = obj.Get("source_level"); v && v->IsInt())
			fd.sourceLevel = static_cast<int>(v->AsInt());
		if (const auto* v = obj.Get("creates_sources"); v && v->IsBool())
			fd.createsSources = v->AsBool();

		// Texture map (face tiles)
		bool tilesValid = true;
		for (int i = 0; i < 6; ++i) {
			const DataFormat::Value* fv = obj.Get(kFaceNames[i]);
			// fluids.data stores face tiles inside texture_map sub-object
			// fall back to top-level if not present
			if (!fv) {
				const DataFormat::Value* tmVal = obj.Get("texture_map");
				if (tmVal && tmVal->IsObject()) fv = tmVal->AsObject().Get(kFaceNames[i]);
			}
			if (!fv || !fv->IsArray()) { tilesValid = false; break; }
			const DataFormat::TypedArray& arr = fv->AsArray();
			if (arr.elements.size() < 2) { tilesValid = false; break; }
			const DataFormat::Value& fx = *arr.elements[0];
			const DataFormat::Value& fy = *arr.elements[1];
			if (!fx.IsInt() || !fy.IsInt()) { tilesValid = false; break; }
			fd.faceTiles[i] = FaceTile{ static_cast<int>(fx.AsInt()), static_cast<int>(fy.AsInt()) };
		}
		if (!tilesValid) {
			std::fprintf(stderr, "Warning: fluid '%s' has invalid texture_map; skipping.\n", fd.name.c_str());
			continue;
		}

		// Interaction entries
		for (const auto& [eKey, eValPtr] : obj.entries) {
			if (eKey != "entry" || !eValPtr || !eValPtr->IsObject()) continue;
			const DataFormat::Object& eo = eValPtr->AsObject();

			FluidInteractionEntry entry;
			if (const auto* v = eo.Get("direction"); v && v->IsTag())
				entry.direction = parseDirection(v->AsTag().name);
			if (const auto* v = eo.Get("change_to"); v && v->IsString())
				entry.changeTo = v->AsString();

			if (const auto* v = eo.Get("fluid"); v && v->IsString()) {
				entry.targetType = FluidInteractionEntry::TargetType::Fluid;
				entry.targetName = v->AsString();
				fd.interactions.push_back(entry);
			} else if (const auto* v = eo.Get("block"); v && v->IsString()) {
				entry.targetType = FluidInteractionEntry::TargetType::Block;
				entry.targetName = v->AsString();
				fd.interactions.push_back(entry);
			} else if (const auto* v = eo.Get("group"); v && v->IsTag()) {
				entry.targetType = FluidInteractionEntry::TargetType::Group;
				entry.targetName = v->AsTag().name;
				fd.interactions.push_back(entry);
			}
		}

		// Generation rules (inside generation_rules sub-object)
		if (const auto* grVal = obj.Get("generation_rules"); grVal && grVal->IsObject()) {
			const DataFormat::Object& gr = grVal->AsObject();
			if (const auto* v = gr.Get("temp"); v && v->IsFloatRange()) {
				fd.tempMin = static_cast<float>(v->AsFloatRange().lo);
				fd.tempMax = static_cast<float>(v->AsFloatRange().hi);
				fd.hasGenRules = true;
			}
			if (const auto* v = gr.Get("elevation"); v && v->IsIntRange()) {
				fd.elevMin = static_cast<int>(v->AsIntRange().lo);
				fd.elevMax = static_cast<int>(v->AsIntRange().hi);
				fd.hasGenRules = true;
			}
		}

		// Group membership
		if (const auto* v = obj.Get("group"); v && v->IsTag())
			fd.groups.push_back(v->AsTag().name);

		if (!registry.Register(fd)) {
			std::fprintf(stderr, "Fluid registration failed for ID %u (%s).\n", fd.fluidID, fd.name.c_str());
		}
	}
	return !registry.Fluids().empty();
}
