#include "WorldSession.hpp"
#include "MenuSession.hpp"

#include <cstdio>
#include <filesystem>
#include <vector>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/norm.hpp>

#include <imgui.h>

#include "AssetLoader.hpp"
#include "Camera.hpp"
#include "GameUI.hpp"
#include "Grid.hpp"
#include "HandModel.hpp"
#include "Keybinds.hpp"
#include "Physics.hpp"
#include "Shader.hpp"
#include "StructureFile.hpp"
#include "TerrainGen.hpp"
#include "WorldFile.hpp"

void WorldSession::Enter(AppContext& ctx, const WorldFile::Header& header, const std::string& savePath) {
	ctx.currentSeed        = header.seed;
	ctx.currentWorldHeader = header;
	if (!savePath.empty()) {
		ctx.worldSavePath = savePath;
	} else {
		const std::string filename = "worlds/" + std::to_string(ctx.currentSeed) + ".world";
		if(const char* base = SDL_GetBasePath())
			ctx.worldSavePath = std::string(base) + filename;
		else
			ctx.worldSavePath = filename;
	}

	ctx.grid->RebuildAll(*ctx.blockAtlas);

	if (!handModel_.Initialize()) {
		std::fprintf(stderr, "Warning: HandModel initialization failed.\n");
	}

	if (skinShader_.Program() == 0) {
		const auto resolveAsset = [](const std::string& rel) -> std::string {
			if(const char* base = SDL_GetBasePath())
				return std::string(base) + rel;
			return rel;
		};
		const std::string skinVert = resolveAsset("assets/shaders/skin.vert");
		const std::string skinFrag = resolveAsset("assets/shaders/skin.frag");
		if (!skinShader_.LoadFromFiles(skinVert, skinFrag)) {
			std::fprintf(stderr, "Warning: HandModel skin shader load failed.\n");
		}
	}

	{
		const auto resolveAsset = [](const std::string& rel) -> std::string {
			if(const char* base = SDL_GetBasePath())
				return std::string(base) + rel;
			return rel;
		};
		const std::string skinPath = resolveAsset("assets/Nadeli0.png");
		handModel_.LoadSkin(skinPath);
	}

	if (header.hasPlayerPos && !ctx.isStructureSession) {
		ctx.physics->teleportTo(*ctx.player, header.playerPos, ctx.camera);
	} else {
		float spawnYf;
		if (ctx.isStructureSession) {
			spawnYf = static_cast<float>(ctx.structureOrigin.y) + 2.0f;
		} else if (header.worldType == WorldFile::WorldType::Superflat) {
			int surfY = 0;
			for (const auto& l : header.superflatLayers) surfY += l.thickness;
			spawnYf = static_cast<float>(surfY) + 2.0f;
		} else {
			const TerrainGen::Params spawnParams{ header.seed };
			spawnYf = static_cast<float>(
				TerrainGen::SampleSurfaceY(0.5f, 0.5f, spawnParams)) + 2.0f;
		}
		ctx.physics->teleportTo(*ctx.player, {0.5f, spawnYf, 0.5f}, ctx.camera);
	}

	isFlying       = false;
	lastSpaceTapMs = 0;

	if(!SDL_SetWindowRelativeMouseMode(ctx.window, true)) {
		std::fprintf(stderr, "Warning: could not enable relative mouse mode: %s\n", SDL_GetError());
	}
}

void WorldSession::ProcessEvent(const SDL_Event& event, AppContext& ctx) {
	if(ctx.gameState != GameState::PLAYING && ctx.gameState != GameState::PAUSE_MENU) return;

	if(event.type == SDL_EVENT_KEY_DOWN && ctx.gameState == GameState::PLAYING) {
		const SDL_Scancode sc      = event.key.scancode;
		const bool* const  kbState = SDL_GetKeyboardState(nullptr);
		const bool nonRepeat = !event.key.repeat;

		if(ChordPressed(sc, kbState, ctx.keybinds->pause)) {
			ctx.gameState = GameState::PAUSE_MENU;
			SDL_SetWindowRelativeMouseMode(ctx.window, false);
		}
		if(nonRepeat && ChordPressed(sc, kbState, ctx.keybinds->ui_toggle))   { showGameplayUi = !showGameplayUi; }
		if(nonRepeat && ChordPressed(sc, kbState, ctx.keybinds->screenshot))  { ctx.screenshotRequested = true; }
		if(nonRepeat && ChordPressed(sc, kbState, ctx.keybinds->view_toggle)) { thirdPersonView = !thirdPersonView; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_toggle))         { debugView          = !debugView; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_wireframe))      { debugWireframe     = !debugWireframe; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_block))          { debugLookedAtBlock = !debugLookedAtBlock; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_face))           { debugLookedAtFace  = !debugLookedAtFace; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_data))           { debugLookedAtData  = !debugLookedAtData; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_wireframe_only)) { debugWireframeOnly = !debugWireframeOnly; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_stance))         { debugStance        = !debugStance; }
		if(ChordPressed(sc, kbState, ctx.keybinds->debug_velocity))       { debugVelocity      = !debugVelocity; }

		if(ChordPressed(sc, kbState, ctx.keybinds->debug_reload)) {
			// Hot-reload physics constants
			PhysicsConstants reloaded;
			if(LoadPhysicsConstants(ctx.physicsConstantsPath, reloaded)) {
				*ctx.physicsConstants = reloaded;
				ctx.physics->SetConstants(*ctx.physicsConstants);
				std::fprintf(stderr, "Hot-reloaded physics_constants.data\n");
			} else {
				std::fprintf(stderr, "Warning: hot-reload failed for '%s'\n", ctx.physicsConstantsPath.c_str());
			}
			// Hot-reload blocks
			ctx.blockRegistry->Clear();
			if(LoadBlocks(ctx.blocksDataPath, ctx.blockAtlas, *ctx.blockRegistry)) {
				ctx.grid->RebuildVisibility();
				std::fprintf(stderr, "Hot-reloaded blocks.data\n");
			} else {
				std::fprintf(stderr, "Warning: hot-reload failed for '%s'\n", ctx.blocksDataPath.c_str());
			}
			// Hot-reload keybinds
			Keybinds reloadedKeybinds;
			if(LoadKeybinds(ctx.keybindsDataPath, reloadedKeybinds)) {
				*ctx.keybinds = reloadedKeybinds;
				std::fprintf(stderr, "Hot-reloaded keybinds.data\n");
			} else {
				std::fprintf(stderr, "Warning: hot-reload failed for '%s'\n", ctx.keybindsDataPath.c_str());
			}
		}

		if(ChordPressed(sc, kbState, ctx.keybinds->debug_save)) {
			if(ctx.isStructureSession) {
				StructureFile::Header sh;
				sh.origin = ctx.structureOrigin;
				namespace fs = std::filesystem;
				fs::create_directories(fs::path(ctx.worldSavePath).parent_path());
				if(StructureFile::Save(ctx.worldSavePath, sh, *ctx.grid)) {
					std::fprintf(stderr, "Structure saved to: %s\n", ctx.worldSavePath.c_str());
				} else {
					std::fprintf(stderr, "Warning: structure save failed for '%s'\n", ctx.worldSavePath.c_str());
				}
			} else {
				ctx.currentWorldHeader.playerPos    = ctx.player->position;
				ctx.currentWorldHeader.hasPlayerPos = true;
				std::filesystem::create_directories(ctx.worldsDir);
				if(WorldFile::Save(ctx.worldSavePath, ctx.currentWorldHeader, *ctx.grid)) {
					std::fprintf(stderr, "World saved to: %s\n", ctx.worldSavePath.c_str());
				} else {
					std::fprintf(stderr, "Warning: world save failed for '%s'\n", ctx.worldSavePath.c_str());
				}
			}
		}

		if(ChordPressed(sc, kbState, ctx.keybinds->debug_load)) {
			if(ctx.isStructureSession) {
				StructureFile::Header sh;
				if(StructureFile::Load(ctx.worldSavePath, sh, *ctx.grid)) {
					ctx.structureOrigin = sh.origin;
					ctx.grid->RebuildVisibility();
					std::fprintf(stderr, "Structure loaded from: %s\n", ctx.worldSavePath.c_str());
				} else {
					std::fprintf(stderr, "Warning: structure load failed for '%s'\n", ctx.worldSavePath.c_str());
				}
			} else {
				WorldFile::Header wfh;
				if(WorldFile::Load(ctx.worldSavePath, wfh, *ctx.grid)) {
					ctx.currentWorldHeader = wfh;
					ctx.grid->RebuildVisibility();
					std::fprintf(stderr, "World loaded from: %s (seed %lld)\n", ctx.worldSavePath.c_str(), (long long)wfh.seed);
				} else {
					std::fprintf(stderr, "Warning: world load failed for '%s'\n", ctx.worldSavePath.c_str());
				}
			}
		}

		for(int i = 0; i < 9; ++i) {
			if(ChordPressed(sc, kbState, ctx.keybinds->hotbar[i])) {
				ctx.hotbar->SelectSlot(i);
				break;
			}
		}

		if(ctx.keybinds->crawl_toggle.isSequence &&
		   ChordPressed(sc, kbState, ctx.keybinds->crawl_toggle)) {
			crawlToggleThisFrame = true;
		}

		if(ctx.isStructureSession && sc == SDL_SCANCODE_SPACE && !event.key.repeat) {
			const uint64_t now = SDL_GetTicks();
			if(now - lastSpaceTapMs < 300u) {
				isFlying           = !isFlying;
				ctx.player->velocity.y = 0.0f;
				lastSpaceTapMs     = 0u;
			} else {
				lastSpaceTapMs = now;
			}
		}
	}

	if(ctx.gameState == GameState::PLAYING) {
		if(event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			// face neighbour offsets: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
			constexpr glm::ivec3 kFaceOffset[6] = {
				{ 1,  0,  0}, {-1,  0,  0},
				{ 0,  1,  0}, { 0, -1,  0},
				{ 0,  0,  1}, { 0,  0, -1},
			};
			const Grid::LookedAtResult hit = ctx.grid->QueryLookedAt(*ctx.camera);

			if(event.button.button == SDL_BUTTON_LEFT) {
				if(hit.hit) {
					ctx.grid->RemoveBlock(hit.blockPos);
					handModel_.TriggerSwing();
				}
			} else if(event.button.button == SDL_BUTTON_MIDDLE) {
				if(hit.hit) {
					ctx.hotbar->SetSlot(ctx.hotbar->SelectedSlot(), hit.blockID);
					handModel_.TriggerPoint();
				}
			} else if(event.button.button == SDL_BUTTON_RIGHT) {
				if(hit.hit && hit.faceIndex >= 0) {
					const glm::ivec3 placePos = hit.blockPos + kFaceOffset[hit.faceIndex];
					if(!ctx.grid->HasBlockAt(placePos) && placePos.y < 513) {
						const uint32_t selectedBlockID = ctx.hotbar->CurrentBlockID();
						if(selectedBlockID != 0u || ctx.hotbar->SlotHasBlock(ctx.hotbar->SelectedSlot())) {
							if(ctx.physics->CanPlaceBlockAt(*ctx.player, *ctx.camera, placePos)) {
								uint8_t rotation = 0;
								if(const BlockData* bd = ctx.blockRegistry->Get(selectedBlockID); bd && bd->canRotate.any()) {
									// hit.faceIndex: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
									// rotation (upDir): 0=+Y(upright), 1=-Y(flip), 2=+X(tilt R), 3=-X(tilt L), 4=+Z(tilt fwd), 5=-Z(tilt back)
									switch(hit.faceIndex) {
										case 0: rotation = bd->canRotate.z ? 2u : 0u; break;
										case 1: rotation = bd->canRotate.z ? 3u : 0u; break;
										case 2: rotation = 0u; break;
										case 3: rotation = (bd->canRotate.x || bd->canRotate.z) ? 1u : 0u; break;
										case 4: rotation = bd->canRotate.x ? 4u : 0u; break;
										case 5: rotation = bd->canRotate.x ? 5u : 0u; break;
										default: break;
									}
								}
								ctx.grid->AddBlock(placePos.x, placePos.y, placePos.z, selectedBlockID, rotation);
								handModel_.TriggerSwing();
							}
						}
					}
				}
			}
		}

		if(event.type == SDL_EVENT_MOUSE_WHEEL) {
			if(event.wheel.y > 0)      { ctx.hotbar->SelectPrev(); }
			else if(event.wheel.y < 0) { ctx.hotbar->SelectNext(); }
		}

		if(event.type == SDL_EVENT_MOUSE_MOTION) {
			mouseDeltaX += event.motion.xrel;
			mouseDeltaY += event.motion.yrel;
		}
	}
}

bool WorldSession::Frame(double dt, int displayedFps, int winW, int winH, AppContext& ctx) {
	if(ctx.gameState == GameState::PLAYING) {
		ctx.camera->UpdateFromMouseDelta(mouseDeltaX, mouseDeltaY);
	}
	mouseDeltaX = 0.0f;
	mouseDeltaY = 0.0f;

	const bool* const keys       = SDL_GetKeyboardState(nullptr);
	if(!ctx.keybinds->crawl_toggle.isSequence) {
		const bool crawlCombo = ChordHeld(keys, ctx.keybinds->crawl_toggle);
		crawlToggleThisFrame  = crawlCombo && !prevCrawlComboDown;
		prevCrawlComboDown    = crawlCombo;
	} else {
		prevCrawlComboDown = false;
		
	}

	if(ctx.gameState == GameState::PLAYING) {
		glm::vec3 forward = ctx.camera->Forward();
		forward.y = 0.0f;
		forward   = glm::normalize(forward);
		const glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));

		glm::vec3 moveDir(0.0f);
		if(ChordHeld(keys, ctx.keybinds->move_forward)) moveDir += forward;
		if(ChordHeld(keys, ctx.keybinds->move_back))    moveDir -= forward;
		if(ChordHeld(keys, ctx.keybinds->move_right))   moveDir += right;
		if(ChordHeld(keys, ctx.keybinds->move_left))    moveDir -= right;
		if(glm::length2(moveDir) > 0.0000001f) moveDir = glm::normalize(moveDir);

		const glm::vec3 desiredHV = moveDir * ctx.physicsConstants->moveSpeed;

		if(ctx.isStructureSession && isFlying) {
			const bool flyUp   = ChordHeld(keys, ctx.keybinds->jump);
			const bool flyDown = ChordHeld(keys, ctx.keybinds->crouch);
			float vertVel = 0.0f;
			if(flyUp && !flyDown)      vertVel =  ctx.physicsConstants->moveSpeed;
			else if(flyDown && !flyUp) vertVel = -ctx.physicsConstants->moveSpeed;
			ctx.physics->StepEntityFlying(
				*ctx.player,
				static_cast<float>(dt),
				glm::vec3(desiredHV.x, vertVel, desiredHV.z)
			);
		} else {
			const bool crouchHeld = ChordHeld(keys, ctx.keybinds->crouch);
			ctx.physics->StepEntityEuler(
				*ctx.player,
				static_cast<float>(dt),
				desiredHV,
				ChordHeld(keys, ctx.keybinds->jump),
				crouchHeld,
				crawlToggleThisFrame,
				*ctx.physicsConstants
			);
		}
		crawlToggleThisFrame = false;
		ctx.physics->StepBlockGravity(static_cast<float>(dt));
		ctx.physics->UpdateFallingBlocks(static_cast<float>(dt));
		ctx.physics->ForceEntityUpIfInsideBlock(*ctx.player);

		if (ctx.player->position.y < -16.0f) {
			float safeY = 2.0f;
			if (ctx.currentWorldHeader.worldType == WorldFile::WorldType::Superflat) {
				int surfY = 0;
				for (const auto& layer : ctx.currentWorldHeader.superflatLayers)
					surfY += layer.thickness;
				safeY = static_cast<float>(surfY) + 2.0f;
			} else {
				TerrainGen::Params spawnParams;
				spawnParams.seed = ctx.currentSeed;
				safeY = static_cast<float>(
					TerrainGen::SampleSurfaceY(0.5f, 0.5f, spawnParams)) + 2.0f;
			}
			ctx.physics->teleportTo(*ctx.player, {0.5f, safeY, 0.5f}, ctx.camera);
		}

		ctx.camera->position = ctx.player->position;
	}

	handModel_.Update(static_cast<float>(dt));

	if (ctx.hotbar->SlotHasBlock(ctx.hotbar->SelectedSlot()))
		handModel_.SetBlock(ctx.hotbar->CurrentBlockID(), *ctx.blockRegistry, *ctx.blockAtlas);
	else
		handModel_.ClearBlock();

	// 3D rendering
	const float aspect       = (winH > 0) ? static_cast<float>(winW) / static_cast<float>(winH) : 1.0f;
	const glm::mat4 proj     = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
	glm::mat4 view = ctx.camera->View();
	if(thirdPersonView) {
		const glm::vec3 lookAtPos = ctx.player->position + glm::vec3(0.0f, ctx.player->eyeFromFeet * 0.9f, 0.0f);
		const glm::vec3 offset    = (-ctx.camera->Forward() * 4.0f) + glm::vec3(0.0f, 1.2f, 0.0f);
		const glm::vec3 camPos    = lookAtPos + offset;
		view = glm::lookAt(camPos, lookAtPos, glm::vec3(0.0f, 1.0f, 0.0f));
	}

	if(!(debugView && debugWireframeOnly)) {
		ctx.grid->Draw(*ctx.defaultShader, *ctx.blockAtlas, proj, view);

		std::vector<Grid::FloatBlock> fallingVisual;
		for(const Physics::FallingBlock& fb : ctx.physics->GetFallingBlocks()) {
			fallingVisual.push_back({fb.pos, fb.blockID});
		}
		ctx.grid->DrawFloatBlocks(fallingVisual, *ctx.defaultShader, *ctx.blockAtlas, proj, view);
	}

	glClear(GL_DEPTH_BUFFER_BIT);

	// Draw first-person hand model
	if (ctx.gameState == GameState::PLAYING && showGameplayUi && !thirdPersonView) {
		handModel_.Draw(*ctx.defaultShader, skinShader_, *ctx.blockAtlas, winW, winH);
	}

	// ImGui overlay
	ctx.debugOverlay->NewFrame();

	bool returnedToMenu = false;

	if(ctx.gameState == GameState::PLAYING) {
		if(showGameplayUi && debugView) {
			if(debugWireframe || debugWireframeOnly) {
				ctx.grid->DrawWireframe(*ctx.wireframeShader, proj, view);
			}
			if(debugLookedAtBlock) { ctx.grid->DrawLookedAtBlock(*ctx.wireframeShader, *ctx.camera, proj, view); }
			if(debugLookedAtFace)  { ctx.grid->DrawLookedAtFace(*ctx.wireframeShader, *ctx.camera, proj, view); }

			ImGui::SetNextWindowPos(ImVec2{10.0f, 10.0f}, ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.6f);
			ImGui::Begin("Debug", nullptr,
				ImGuiWindowFlags_NoDecoration       |
				ImGuiWindowFlags_AlwaysAutoResize   |
				ImGuiWindowFlags_NoSavedSettings    |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav              |
				ImGuiWindowFlags_NoMove);

			ImGui::Text("FPS: %d", displayedFps);
			ImGui::Separator();
			ImGui::Text("Seed: %lld", (long long)ctx.currentSeed);
			{
				std::string biomeStr;
				if (!ctx.currentWorldHeader.superflatLayers.empty()) {
					biomeStr = "none";
				} else {
					TerrainGen::Params bp;
					bp.seed = ctx.currentSeed;
					if (ctx.currentWorldHeader.worldType == WorldFile::WorldType::SingleBiome)
						bp.forceBiome = ctx.currentWorldHeader.singleBiome;
					biomeStr = TerrainGen::GetBiomeAt(
						ctx.player->position.x, ctx.player->position.z,
						ctx.biomeRegistry, bp);
					const BiomeData* bd = ctx.biomeRegistry
						? ctx.biomeRegistry->GetById(biomeStr) : nullptr;
					if (bd) biomeStr = bd->displayName;
				}
				ImGui::Text("Biome: %s", biomeStr.c_str());
			}
			{
				TerrainGen::Params bp;
				bp.seed = ctx.currentSeed;
				if (ctx.currentWorldHeader.worldType == WorldFile::WorldType::SingleBiome)
					bp.forceBiome = ctx.currentWorldHeader.singleBiome;
				if (ctx.currentWorldHeader.superflatLayers.empty()) {
					const float temperature = TerrainGen::SampleTemperature(
						ctx.player->position.x, ctx.player->position.z, bp);
					const int surfaceElevation = TerrainGen::SampleSurfaceY(
						ctx.player->position.x, ctx.player->position.z, bp);
					ImGui::Text("Temperature: %.3f", temperature);
					ImGui::Text("Elevation: %d", surfaceElevation);
				} else {
					ImGui::TextDisabled("Temperature: N/A");
					ImGui::TextDisabled("Elevation: N/A");
				}
			}
			ImGui::Separator();
			ImGui::Text("Position: %.2f  %.2f  %.2f",
				ctx.player->position.x, ctx.player->position.y, ctx.player->position.z);

			if(debugLookedAtData) {
				const Grid::LookedAtResult hit = ctx.grid->QueryLookedAt(*ctx.camera);
				ImGui::Separator();
				if(hit.hit) {
					const char* const blockName = (hit.blockData != nullptr) ? hit.blockData->name.c_str() : "UNKNOWN";
					ImGui::Text("Block Pos: [%d, %d, %d]", hit.blockPos.x, hit.blockPos.y, hit.blockPos.z);
					ImGui::Text("Face:      %d", hit.faceIndex);
					ImGui::Text("Name:      %s", blockName);
					ImGui::Text("ID:        %u", static_cast<unsigned int>(hit.blockID));
				} else {
					ImGui::TextDisabled("No block in view");
				}
			}

			if(debugStance) {
				ImGui::Separator();
				ImGui::Text("Stance: %s", ctx.player->getPosture());
			}

			if(debugVelocity) {
				ImGui::Separator();
				ImGui::Text("Velocity: %.3f  %.3f  %.3f",
					ctx.player->velocity.x, ctx.player->velocity.y, ctx.player->velocity.z);
			}

			ImGui::End();
		}
		if(showGameplayUi) {
			ctx.hotbar->Draw(*ctx.blockRegistry, winW, winH);
		}

	} else if(ctx.gameState == GameState::PAUSE_MENU) {
		bool wantResume   = false;
		bool wantSaveQuit = false;
		bool wantSettings = false;
		DrawPauseMenu(wantSaveQuit, wantResume, wantSettings, winW, winH);

		if(wantResume) {
			ctx.gameState = GameState::PLAYING;
			SDL_SetWindowRelativeMouseMode(ctx.window, true);
		}
		if(wantSettings) {
			ctx.menuSession->settingsPage        = SettingsPage::MAIN;
			ctx.menuSession->listeningKeybindIdx = -1;
			ctx.menuSession->editedKeybinds      = *ctx.keybinds;
			ctx.menuSession->settingsReturnState = GameState::PAUSE_MENU;
			ctx.gameState = GameState::SETTINGS_MENU;
		}
		if(wantSaveQuit) {
			if(ctx.isStructureSession) {
				StructureFile::Header sh;
				sh.origin = ctx.structureOrigin;
				namespace fs = std::filesystem;
				fs::create_directories(fs::path(ctx.worldSavePath).parent_path());
				if(!StructureFile::Save(ctx.worldSavePath, sh, *ctx.grid)) {
					std::fprintf(stderr, "Warning: structure save failed for '%s'\n", ctx.worldSavePath.c_str());
				}
				ctx.isStructureSession = false;
			} else {
				ctx.currentWorldHeader.playerPos    = ctx.player->position;
				ctx.currentWorldHeader.hasPlayerPos = true;
				std::filesystem::create_directories(ctx.worldsDir);
				if(!WorldFile::Save(ctx.worldSavePath, ctx.currentWorldHeader, *ctx.grid)) {
					std::fprintf(stderr, "Warning: world save failed for '%s'\n", ctx.worldSavePath.c_str());
				}
			}
			ctx.grid->Clear();
			ctx.gameState  = GameState::MAIN_MENU;
			returnedToMenu = true;
		}
	}

	ctx.debugOverlay->Render();
	return returnedToMenu;
}
