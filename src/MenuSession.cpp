#include "MenuSession.hpp"
#include "WorldSession.hpp"

#include <cstdio>
#include <filesystem>

#include <SDL3/SDL.h>

#include "GameUI.hpp"
#include "Grid.hpp"
#include "Hotbar.hpp"
#include "TerrainGen.hpp"
#include "WorldFile.hpp"

namespace {
	enum BLOCKS { GRASS = 0, DIRT, STONE, ANDESITE, SAND };
}

bool MenuSession::Frame(int winW, int winH, AppContext& ctx, WorldSession& worldSession) {
	ctx.debugOverlay->NewFrame();

	bool wantQuit = false;

	if(ctx.gameState == GameState::MAIN_MENU) {
		GameState nextState = GameState::MAIN_MENU;
		DrawMainMenu(nextState, wantQuit, winW, winH);
		ctx.gameState = nextState;
		if(ctx.gameState == GameState::WORLDS_MENU) {
			worldEntries.clear();
		}

	} else if(ctx.gameState == GameState::SETTINGS_MENU) {
		bool wantBack = false;
		DrawSettingsMenu(wantBack, winW, winH);
		if(wantBack) { ctx.gameState = GameState::MAIN_MENU; }

	} else if(ctx.gameState == GameState::WORLDS_MENU) {
		bool         wantLoad = false, wantNew = false, wantDelete = false, wantBack = false;
		std::string  loadPath, loadName;
		WorldFile::Header loadHeader;

		DrawWorldsMenu(ctx.worldsDir, worldEntries, selectedIdx,
		               wantLoad, loadPath, loadName, loadHeader,
		               wantNew, wantDelete, wantBack,
		               winW, winH);

		if(wantBack) { ctx.gameState = GameState::MAIN_MENU; }
		if(wantNew)  { ctx.gameState = GameState::NEW_WORLD_MENU; newWorldParams = {}; }

		if(wantDelete && selectedIdx >= 0 && selectedIdx < static_cast<int>(worldEntries.size())) {
			std::filesystem::remove(worldEntries[selectedIdx].path);
			worldEntries.clear();
			selectedIdx = -1;
		}

		if(wantLoad) {
			ctx.grid->Clear();
			WorldFile::Header h;
			if(WorldFile::Load(loadPath, h, *ctx.grid)) {
				ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
				ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
				ctx.hotbar->SetSlot(4, SAND);
				worldSession.Enter(ctx, h);
				ctx.gameState = GameState::PLAYING;
			} else {
				std::fprintf(stderr, "Warning: failed to load world '%s'\n", loadPath.c_str());
			}
		}

	} else if(ctx.gameState == GameState::NEW_WORLD_MENU) {
		bool wantCreate = false, wantBack = false;
		DrawNewWorldMenu(newWorldParams, wantCreate, wantBack, winW, winH);

		if(wantBack) { ctx.gameState = GameState::WORLDS_MENU; }

		if(wantCreate) {
			WorldFile::Header h = newWorldParams.MakeHeader();
			if(h.seed == 0) {
				h.seed = static_cast<int32_t>(SDL_GetPerformanceCounter() & 0x7FFFFFFF);
				if(h.seed == 0) h.seed = 1;
			}

			ctx.grid->Clear();
			TerrainGen::Params genParams;
			genParams.seed = h.seed;
			if(h.worldType == WorldFile::WorldType::Superflat) {
				genParams.noiseScale      = 0.0001f;
				genParams.heightAmplitude = 0;
				genParams.baseHeight      = 0;
			}
			TerrainGen::Generate(*ctx.grid, *ctx.blockRegistry, genParams);

			ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
			ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
			ctx.hotbar->SetSlot(4, SAND);

			std::filesystem::create_directories(ctx.worldsDir);
			const std::string newPath = ctx.worldsDir + std::to_string(h.seed) + ".world";
			WorldFile::Save(newPath, h, *ctx.grid);

			worldSession.Enter(ctx, h);
			ctx.gameState = GameState::PLAYING;
		}
	}

	ctx.debugOverlay->Render();
	return wantQuit;
}
