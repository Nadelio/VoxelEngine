#include "DebugCLI.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL3/SDL.h>
#include <imgui.h>

#include "AppContext.hpp"

namespace {
    static std::vector<std::string> Tokenize(const char* input) {
        std::vector<std::string> tokens;
        std::string tok;
        for (const char* p = input; *p != '\0'; ++p) {
            const char c = *p;
            if (c == '(' || c == ')') continue;
            if (c == ' ' || c == '\t' || c == ',') {
                if (!tok.empty()) { tokens.push_back(std::move(tok)); tok.clear(); }
            } else {
                tok += c;
            }
        }
        if (!tok.empty()) tokens.push_back(std::move(tok));
        return tokens;
    }

    static bool ParseUint32(const std::string& s, uint32_t& out) {
        if (s.empty()) return false;
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        }
        try {
            const unsigned long v = std::stoul(s);
            out = static_cast<uint32_t>(v);
            return true;
        } catch (...) {
            return false;
        }
    }
}

void DebugCLI::Open(AppContext& ctx) {
	isOpen       = true;
	focusNext_   = true;
	hasResult_   = false;
	inputBuf_[0] = '\0';
	SDL_SetWindowRelativeMouseMode(ctx.window, false);
}

void DebugCLI::Close(AppContext& ctx) {
	isOpen = false;
	SDL_SetWindowRelativeMouseMode(ctx.window, true);
}

void DebugCLI::Draw(int winW, int winH, AppContext& ctx) {
	if (!isOpen) return;

	constexpr float kSlotCount  = 9.0f;
	constexpr float kSlotSize   = 52.0f;
	constexpr float kSlotPad    = 4.0f;
	constexpr float kMarginBot  = 16.0f;
	constexpr float kEdgeGap    = 8.0f;

	const float hotbarW = kSlotCount * kSlotSize + (kSlotCount - 1.0f) * kSlotPad;
	const float hotbarX = (static_cast<float>(winW) - hotbarW) * 0.5f;

	const float barY = static_cast<float>(winH) - kMarginBot - kSlotSize;
	const float barH = kSlotSize;
	const float barX = kEdgeGap;
	const float barW = hotbarX - kEdgeGap - barX;

	if (hasResult_) {
		constexpr float kResultH   = 24.0f;
		constexpr float kResultGap = 12.0f;
		ImGui::SetNextWindowPos(ImVec2{barX, barY - kResultH - kResultGap}, ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2{barW, kResultH}, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.75f);
		ImGui::Begin("##CLIResult", nullptr,
			ImGuiWindowFlags_NoDecoration      |
			ImGuiWindowFlags_NoNav              |
			ImGuiWindowFlags_NoMove             |
			ImGuiWindowFlags_NoSavedSettings    |
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoInputs);
		ImGui::TextUnformatted(resultBuf_);
		ImGui::End();
	}

	ImGui::SetNextWindowPos(ImVec2{barX, barY}, ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2{barW, barH}, ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.85f);
	ImGui::Begin("##DebugCLI", nullptr,
		ImGuiWindowFlags_NoDecoration      |
		ImGuiWindowFlags_NoNav              |
		ImGuiWindowFlags_NoMove             |
		ImGuiWindowFlags_NoSavedSettings    |
		ImGuiWindowFlags_NoFocusOnAppearing);

	if (focusNext_) {
		ImGui::SetKeyboardFocusHere();
		focusNext_ = false;
	}

	ImGui::SetNextItemWidth(barW - 16.0f);
	const bool entered = ImGui::InputText("##CLIInput", inputBuf_, sizeof(inputBuf_),
		ImGuiInputTextFlags_EnterReturnsTrue);

	if (entered) {
		if (inputBuf_[0] != '\0') {
			Execute(inputBuf_, ctx);
		}
		inputBuf_[0] = '\0';
		focusNext_ = true;
	}

	ImGui::End();
}

void DebugCLI::Execute(const char* cmd, AppContext& ctx) {
	const auto tokens = Tokenize(cmd);
	if (tokens.empty()) { hasResult_ = false; return; }

	const std::string& verb = tokens[0];

	if (verb == "set") {
		if (tokens.size() < 5) {
			std::snprintf(resultBuf_, sizeof(resultBuf_),
				"Usage: set <block/fluid id> (x,y,z)");
			hasResult_ = true; return;
		}

		int cx = 0, cy = 0, cz = 0;
		try {
			cx = std::stoi(tokens[2]);
			cy = std::stoi(tokens[3]);
			cz = std::stoi(tokens[4]);
		} catch (...) {
			std::snprintf(resultBuf_, sizeof(resultBuf_), "Error: invalid coordinates");
			hasResult_ = true; return;
		}

		const std::string& idStr = tokens[1];
		uint32_t parsedID = 0;
		const bool isNum  = ParseUint32(idStr, parsedID);

		uint32_t blockID   = 0;
		bool     foundBlock = false;
		if (isNum) {
			if (parsedID == 0 || ctx.blockRegistry->Get(parsedID)) {
				blockID    = parsedID;
				foundBlock = true;
			}
		} else {
			for (const auto& [id, bd] : ctx.blockRegistry->Blocks()) {
				if (bd.name == idStr) { blockID = id; foundBlock = true; break; }
			}
		}

		uint32_t fluidID   = 0;
		bool     foundFluid = false;
		if (!foundBlock && ctx.fluidRegistry) {
			if (isNum) {
				if (ctx.fluidRegistry->Get(parsedID)) {
					fluidID    = parsedID;
					foundFluid = true;
				}
			} else {
				const FluidData* fd = ctx.fluidRegistry->GetByName(idStr);
				if (fd) { fluidID = fd->fluidID; foundFluid = true; }
			}
		}

		ctx.grid->RemoveBlock({cx, cy, cz});
		if (ctx.fluidGrid) ctx.fluidGrid->RemoveFluid(cx, cy, cz);

		if (foundBlock && blockID != 0) {
			ctx.grid->AddBlock(cx, cy, cz, blockID);
			const BlockData* bd = ctx.blockRegistry->Get(blockID);
			std::snprintf(resultBuf_, sizeof(resultBuf_),
				"Set block %u (%s) at (%d,%d,%d)",
				blockID, bd ? bd->name.c_str() : "?", cx, cy, cz);
		} else if (foundBlock) {
			std::snprintf(resultBuf_, sizeof(resultBuf_), "Cleared (%d,%d,%d)", cx, cy, cz);
		} else if (foundFluid && ctx.fluidGrid) {
			const FluidData* fd = ctx.fluidRegistry->Get(fluidID);
			const uint8_t srcLevel = fd ? static_cast<uint8_t>(fd->sourceLevel) : 8u;
			ctx.fluidGrid->SetFluid(cx, cy, cz, fluidID, srcLevel, /*isSource=*/true);
			std::snprintf(resultBuf_, sizeof(resultBuf_),
				"Set fluid %u (%s) at (%d,%d,%d)",
				fluidID, fd ? fd->name.c_str() : "?", cx, cy, cz);
		} else {
			std::snprintf(resultBuf_, sizeof(resultBuf_),
				"Error: unknown id '%s'", idStr.c_str());
		}
		hasResult_ = true;
	}

	else if (verb == "give") {
		if (tokens.size() < 3) {
			std::snprintf(resultBuf_, sizeof(resultBuf_), "Usage: give block|item <id>");
			hasResult_ = true; return;
		}

		const std::string& idStr = tokens[2];
		uint32_t parsedID = 0;
		const bool isNum  = ParseUint32(idStr, parsedID);

		uint32_t blockID = 0;
		bool     found   = false;
		if (isNum) {
			if (ctx.blockRegistry->Get(parsedID)) { blockID = parsedID; found = true; }
		} else {
			for (const auto& [id, bd] : ctx.blockRegistry->Blocks()) {
				if (bd.name == idStr) { blockID = id; found = true; break; }
			}
		}

		if (found) {
			ctx.hotbar->SetSlot(ctx.hotbar->SelectedSlot(), blockID);
			const BlockData* bd = ctx.blockRegistry->Get(blockID);
			std::snprintf(resultBuf_, sizeof(resultBuf_),
				"Gave block %u (%s) to slot %d",
				blockID, bd ? bd->name.c_str() : "?",
				ctx.hotbar->SelectedSlot() + 1);
		} else {
			std::snprintf(resultBuf_, sizeof(resultBuf_),
				"Error: unknown id '%s'", idStr.c_str());
		}
		hasResult_ = true;
	}

	else if (verb == "tp") {
		if (tokens.size() < 5 || tokens[1] != "self") {
			std::snprintf(resultBuf_, sizeof(resultBuf_), "Usage: tp self (x,y,z)");
			hasResult_ = true; return;
		}

		float tx = 0.f, ty = 0.f, tz = 0.f;
		try {
			tx = std::stof(tokens[2]);
			ty = std::stof(tokens[3]);
			tz = std::stof(tokens[4]);
		} catch (...) {
			std::snprintf(resultBuf_, sizeof(resultBuf_), "Error: invalid coordinates");
			hasResult_ = true; return;
		}

		ctx.physics->teleportTo(*ctx.player, {tx, ty, tz}, ctx.camera);
		std::snprintf(resultBuf_, sizeof(resultBuf_),
			"Teleported to (%.2f, %.2f, %.2f)", tx, ty, tz);
		hasResult_ = true;
	}

	else {
		std::snprintf(resultBuf_, sizeof(resultBuf_),
			"Unknown command '%s'. Commands: set, give, tp", verb.c_str());
		hasResult_ = true;
	}
}
