#include "MenuSession.hpp"
#include "WorldSession.hpp"

#include <cstdio>
#include <filesystem>

#include <SDL3/SDL.h>

#include "AssetLoader.hpp"
#include "GameUI.hpp"
#include "Grid.hpp"
#include "Hotbar.hpp"
#include "Keybinds.hpp"
#include "TerrainGen.hpp"
#include "StructureFile.hpp"
#include "WorldFile.hpp"

namespace {
	enum BLOCKS { GRASS = 0, DIRT, STONE, ANDESITE, SAND, SNOW, ICE, OAK_LEAVES, OAK_LOGS };

	void RememberWindowedSize(AppContext& ctx) {
		if(ctx.windowMode != WindowMode::WINDOWED) return;
		SDL_GetWindowSize(ctx.window, &ctx.windowedWidth, &ctx.windowedHeight);
	}

	void ApplyWindowMode(AppContext& ctx, WindowMode mode) {
		RememberWindowedSize(ctx);

		if(!SDL_SetWindowFullscreen(ctx.window, false)) {
			std::fprintf(stderr, "Warning: could not leave fullscreen: %s\n", SDL_GetError());
		}
		if(!SDL_RestoreWindow(ctx.window)) {
			std::fprintf(stderr, "Warning: could not restore window state: %s\n", SDL_GetError());
		}

		if(mode == WindowMode::WINDOWED) {
			if(!SDL_SetWindowBordered(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not restore window border: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowResizable(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not set window resizable: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowSize(ctx.window, ctx.windowedWidth, ctx.windowedHeight)) {
				std::fprintf(stderr, "Warning: could not set window size: %s\n", SDL_GetError());
			}
		} else if(mode == WindowMode::WINDOWED_FULLSCREEN) {
			if(!SDL_SetWindowBordered(ctx.window, false)) {
				std::fprintf(stderr, "Warning: could not remove window border: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowResizable(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not set window resizable: %s\n", SDL_GetError());
			}
			if(!SDL_MaximizeWindow(ctx.window)) {
				std::fprintf(stderr, "Warning: could not maximize window: %s\n", SDL_GetError());
			}
		} else {
			if(!SDL_SetWindowBordered(ctx.window, false)) {
				std::fprintf(stderr, "Warning: could not remove window border: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowResizable(ctx.window, false)) {
				std::fprintf(stderr, "Warning: could not set window non-resizable: %s\n", SDL_GetError());
			}
			if(!SDL_SetWindowFullscreen(ctx.window, true)) {
				std::fprintf(stderr, "Warning: could not enter fullscreen: %s\n", SDL_GetError());
			}
		}
		ctx.windowMode = mode;
	}
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
		if (settingsPage == SettingsPage::MAIN) {
			bool wantBack      = false;
			bool wantControls  = false;
			bool windowModeChanged = false;
			int windowModeIdx = static_cast<int>(ctx.windowMode);
			std::string pickedBlockAtlas, pickedItemAtlas, pickedResPack, pickedDataPack;
			DrawSettingsMenu(wantBack, wantControls,
			                 pickedBlockAtlas, pickedItemAtlas,
			                 pickedResPack, pickedDataPack,
			                 windowModeIdx, windowModeChanged,
			                 winW, winH);

			if(windowModeChanged && windowModeIdx >= 0 && windowModeIdx <= 2) {
				ApplyWindowMode(ctx, static_cast<WindowMode>(windowModeIdx));
			}

			if (wantBack) {
				ctx.gameState = settingsReturnState;
				settingsReturnState = GameState::MAIN_MENU;
			}
			if (wantControls) {
				settingsPage        = SettingsPage::CONTROLS;
				editedKeybinds      = *ctx.keybinds;
				listeningKeybindIdx = -1;
			}

			if (!pickedBlockAtlas.empty()) {
				if (!ctx.blockAtlas->LoadFromFile(pickedBlockAtlas))
					std::fprintf(stderr, "Warning: failed to load block atlas from '%s'\n",
					             pickedBlockAtlas.c_str());
			}
			if (!pickedItemAtlas.empty()) {
				if (!ctx.itemAtlas->LoadFromFile(pickedItemAtlas))
					std::fprintf(stderr, "Warning: failed to load item atlas from '%s'\n",
					             pickedItemAtlas.c_str());
			}

			if (!pickedResPack.empty()) {
				namespace fs = std::filesystem;
				const std::string blockPath = (fs::path(pickedResPack) / "block_atlas.png").string();
				const std::string itemPath  = (fs::path(pickedResPack) / "item_atlas.png").string();
				if (!ctx.blockAtlas->LoadFromFile(blockPath))
					std::fprintf(stderr, "Warning: resource pack missing block_atlas.png at '%s'\n",
					             blockPath.c_str());
				if (!ctx.itemAtlas->LoadFromFile(itemPath))
					std::fprintf(stderr, "Warning: resource pack missing item_atlas.png at '%s'\n",
					             itemPath.c_str());
			}

			if (!pickedDataPack.empty()) {
				namespace fs = std::filesystem;
				const std::string blocksPath  = (fs::path(pickedDataPack) / "blocks.data").string();
				const std::string physicsPath = (fs::path(pickedDataPack) / "physics_constants.data").string();
				ctx.blockRegistry->Clear();
				if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
					std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n",
					             blocksPath.c_str());
				if (!LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
					std::fprintf(stderr, "Warning: data pack missing physics_constants.data at '%s'\n",
					             physicsPath.c_str());
				else
					ctx.physics->SetConstants(*ctx.physicsConstants);
			}

		} else if (settingsPage == SettingsPage::CONTROLS) {
			bool wantBack = false;
			DrawControlsMenu(editedKeybinds, listeningKeybindIdx, wantBack, winW, winH);
			if (wantBack) {
				if (!SaveKeybinds(ctx.keybindsDataPath, editedKeybinds))
					std::fprintf(stderr, "Warning: could not save keybinds to '%s'\n",
					             ctx.keybindsDataPath.c_str());
				LoadKeybinds(ctx.keybindsDataPath, *ctx.keybinds);
				settingsPage        = SettingsPage::MAIN;
				listeningKeybindIdx = -1;
			}
		}

	} else if(ctx.gameState == GameState::WORLDS_MENU) {
		bool wantLoad = false,
             wantNew = false,
             wantDelete = false,
             wantRename = false,
             wantBack = false;
		std::string  loadPath, loadName, renameNewName;
		WorldFile::Header loadHeader;

		const std::string structuresDir =
		    (std::filesystem::path(ctx.blocksDataPath).parent_path() / "structures").string();
		DrawWorldsMenu(ctx.worldsDir, structuresDir, worldEntries, selectedIdx,
		               wantLoad, loadPath, loadName, loadHeader,
		               wantNew, wantDelete, wantRename, renameNewName, wantBack,
		               winW, winH);

		if(wantBack) { ctx.gameState = GameState::MAIN_MENU; }
		if(wantNew)  { ctx.gameState = GameState::NEW_WORLD_MENU; newWorldParams = {}; }

		if(wantDelete && selectedIdx >= 0 && selectedIdx < static_cast<int>(worldEntries.size())) {
			std::filesystem::remove(worldEntries[selectedIdx].path);
			worldEntries.clear();
			selectedIdx = -1;
		}

		if(wantRename && selectedIdx >= 0 && selectedIdx < static_cast<int>(worldEntries.size())
		   && !worldEntries[selectedIdx].isStructure) {
			namespace fs = std::filesystem;
			const fs::path oldFsPath(worldEntries[selectedIdx].path);
			const fs::path newFsPath = oldFsPath.parent_path() / (renameNewName + ".world");
			if(!fs::exists(newFsPath)) {
				std::error_code ec;
				fs::rename(oldFsPath, newFsPath, ec);
				if(ec) std::fprintf(stderr, "Warning: failed to rename world: %s\n", ec.message().c_str());
			} else {
				std::fprintf(stderr, "Warning: a world named '%s' already exists\n", renameNewName.c_str());
			}
			worldEntries.clear();
			selectedIdx = -1;
		}

		if(wantLoad) {
			namespace fs = std::filesystem;
			const bool loadingStruct =
			    selectedIdx >= 0 && selectedIdx < static_cast<int>(worldEntries.size())
			    && worldEntries[selectedIdx].isStructure;
			if(loadingStruct) {
				ctx.grid->Clear();
				StructureFile::Header sh;
				if(StructureFile::Load(loadPath, sh, *ctx.grid)) {
					ctx.isStructureSession = true;
					ctx.structureOrigin    = sh.origin;
					ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
					ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
					ctx.hotbar->SetSlot(4, SAND);     ctx.hotbar->SetSlot(5, SNOW);
					ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);
					WorldFile::Header dummy;
					worldSession.Enter(ctx, dummy, loadPath);
					ctx.gameState = GameState::PLAYING;
				} else {
					std::fprintf(stderr, "Warning: failed to load structure '%s'\n", loadPath.c_str());
				}
			} else {
				ctx.isStructureSession = false;
				ctx.grid->Clear();
				WorldFile::Header h;
				if(WorldFile::Load(loadPath, h, *ctx.grid)) {
					{
						for (const auto& dp : h.datapacks) {
							const std::string blocksPath  = (fs::path(dp) / "blocks.data").string();
							const std::string physicsPath = (fs::path(dp) / "physics_constants.data").string();
							ctx.blockRegistry->Clear();
							if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
								std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n", blocksPath.c_str());
							if (LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
								ctx.physics->SetConstants(*ctx.physicsConstants);
						}
					}
					ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
					ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
					ctx.hotbar->SetSlot(4, SAND);     ctx.hotbar->SetSlot(5, SNOW);
					ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);
					worldSession.Enter(ctx, h, loadPath);
					ctx.gameState = GameState::PLAYING;
				} else {
					std::fprintf(stderr, "Warning: failed to load world '%s'\n", loadPath.c_str());
				}
			}
		}

	} else if(ctx.gameState == GameState::NEW_WORLD_MENU) {
		bool wantCreate = false, wantBack = false;
		DrawNewWorldMenu(newWorldParams, wantCreate, wantBack, *ctx.biomeRegistry, *ctx.blockRegistry, winW, winH);

		if(wantBack) { ctx.gameState = GameState::WORLDS_MENU; }

		if(wantCreate) {
			namespace fs = std::filesystem;
			if(newWorldParams.worldTypeIdx == 3) {
				const std::string structDir =
				    (fs::path(ctx.blocksDataPath).parent_path() / "structures").string();
				fs::create_directories(structDir);
				const std::string structPath =
				    (fs::path(structDir) / (std::string(newWorldParams.structureNameBuf) + ".struct")).string();

				ctx.grid->Clear();
				for (const auto& dp : newWorldParams.datapacks) {
					const std::string blocksPath  = (fs::path(dp) / "blocks.data").string();
					const std::string physicsPath = (fs::path(dp) / "physics_constants.data").string();
					ctx.blockRegistry->Clear();
					if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
						std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n", blocksPath.c_str());
					if (LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
						ctx.physics->SetConstants(*ctx.physicsConstants);
				}

				ctx.isStructureSession = true;
				ctx.structureOrigin    = {
				    newWorldParams.structureOriginBuf[0],
				    newWorldParams.structureOriginBuf[1],
				    newWorldParams.structureOriginBuf[2]
				};

				ctx.hotbar->SetSlot(0, GRASS);      ctx.hotbar->SetSlot(1, DIRT);
				ctx.hotbar->SetSlot(2, STONE);      ctx.hotbar->SetSlot(3, ANDESITE);
				ctx.hotbar->SetSlot(4, SAND);       ctx.hotbar->SetSlot(5, SNOW);
				ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);

				for(int dx = -2; dx <= 2; ++dx) {
					for(int dz = -2; dz <= 2; ++dz) {
						ctx.grid->AddBlock(dx, ctx.structureOrigin.y, dz, GRASS);
					}
				}

				WorldFile::Header dummy;
				worldSession.Enter(ctx, dummy, structPath);
				ctx.gameState = GameState::PLAYING;
			} else {
				WorldFile::Header h = newWorldParams.MakeHeader(ctx.biomeRegistry);
				if(h.seed == 0) {
					h.seed = static_cast<int64_t>(SDL_GetPerformanceCounter());
					if(h.seed == 0) h.seed = 1;
				}

				ctx.grid->Clear();

				{
					for (const auto& dp : h.datapacks) {
						const std::string blocksPath  = (fs::path(dp) / "blocks.data").string();
						const std::string physicsPath = (fs::path(dp) / "physics_constants.data").string();
						ctx.blockRegistry->Clear();
						if (!LoadBlocks(blocksPath, ctx.blockAtlas, *ctx.blockRegistry))
							std::fprintf(stderr, "Warning: data pack missing blocks.data at '%s'\n", blocksPath.c_str());
						if (LoadPhysicsConstants(physicsPath, *ctx.physicsConstants))
							ctx.physics->SetConstants(*ctx.physicsConstants);
					}
				}

				TerrainGen::Params genParams;
				genParams.seed = h.seed;
				if(h.worldType == WorldFile::WorldType::Superflat) {
					genParams.superflatLayers = h.superflatLayers;
					if(genParams.superflatLayers.empty()) {
						genParams.noiseScale      = 0.0001f;
						genParams.heightAmplitude = 0;
						genParams.baseHeight      = 0;
					}
				} else if(h.worldType == WorldFile::WorldType::SingleBiome) {
					genParams.forceBiome = h.singleBiome;
				}
				genParams.structuresDir =
				    (fs::path(ctx.blocksDataPath).parent_path() / "structures").string();
				TerrainGen::Generate(*ctx.grid, *ctx.blockRegistry, ctx.biomeRegistry, genParams);

				ctx.hotbar->SetSlot(0, GRASS);    ctx.hotbar->SetSlot(1, DIRT);
				ctx.hotbar->SetSlot(2, STONE);    ctx.hotbar->SetSlot(3, ANDESITE);
				ctx.hotbar->SetSlot(4, SAND);     ctx.hotbar->SetSlot(5, SNOW);
				ctx.hotbar->SetSlot(7, OAK_LEAVES); ctx.hotbar->SetSlot(8, OAK_LOGS);

				fs::create_directories(ctx.worldsDir);
				const std::string newPath = ctx.worldsDir + std::to_string(h.seed) + ".world";
				WorldFile::Save(newPath, h, *ctx.grid);

				worldSession.Enter(ctx, h, newPath);
				ctx.gameState = GameState::PLAYING;
			}
		}
	}

	ctx.debugOverlay->Render();
	return wantQuit;
}
