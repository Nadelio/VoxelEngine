#include "WorldSession.hpp"

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
#include "Keybinds.hpp"
#include "Physics.hpp"
#include "Shader.hpp"
#include "TerrainGen.hpp"
#include "WorldFile.hpp"

void WorldSession::Enter(AppContext& ctx, const WorldFile::Header& header, const std::string& savePath) {
	ctx.currentSeed = header.seed;
	if (!savePath.empty()) {
		ctx.worldSavePath = savePath;
	} else {
		const std::string filename = "worlds/" + std::to_string(ctx.currentSeed) + ".world";
		if(const char* base = SDL_GetBasePath())
			ctx.worldSavePath = std::string(base) + filename;
		else
			ctx.worldSavePath = filename;
	}

	ctx.grid->RebuildVisibility();

	const TerrainGen::Params spawnParams{ header.seed };
	const float spawnYf = static_cast<float>(
		TerrainGen::SampleSurfaceY(0.5f, 0.5f, spawnParams)) + 2.0f;
	ctx.physics->teleportTo(*ctx.player, {0.5f, spawnYf, 0.5f}, ctx.camera);

	if(!SDL_SetWindowRelativeMouseMode(ctx.window, true)) {
		std::fprintf(stderr, "Warning: could not enable relative mouse mode: %s\n", SDL_GetError());
	}
}

void WorldSession::ProcessEvent(const SDL_Event& event, AppContext& ctx) {
	if(ctx.gameState != GameState::PLAYING && ctx.gameState != GameState::PAUSE_MENU) return;

	if(event.type == SDL_EVENT_KEY_DOWN && ctx.gameState == GameState::PLAYING) {
		const SDL_Scancode sc      = event.key.scancode;
		const bool* const  kbState = SDL_GetKeyboardState(nullptr);

		if(ChordPressed(sc, kbState, ctx.keybinds->pause)) {
			ctx.gameState = GameState::PAUSE_MENU;
			SDL_SetWindowRelativeMouseMode(ctx.window, false);
		}
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
			WorldFile::Header wfh;
			wfh.seed = ctx.currentSeed;
			std::filesystem::create_directories(ctx.worldsDir);
			if(WorldFile::Save(ctx.worldSavePath, wfh, *ctx.grid)) {
				std::fprintf(stderr, "World saved to: %s\n", ctx.worldSavePath.c_str());
			} else {
				std::fprintf(stderr, "Warning: world save failed for '%s'\n", ctx.worldSavePath.c_str());
			}
		}

		if(ChordPressed(sc, kbState, ctx.keybinds->debug_load)) {
			WorldFile::Header wfh;
			if(WorldFile::Load(ctx.worldSavePath, wfh, *ctx.grid)) {
				ctx.grid->RebuildVisibility();
				std::fprintf(stderr, "World loaded from: %s (seed %d)\n", ctx.worldSavePath.c_str(), wfh.seed);
			} else {
				std::fprintf(stderr, "Warning: world load failed for '%s'\n", ctx.worldSavePath.c_str());
			}
		}

		for(int i = 0; i < 9; ++i) {
			if(ChordPressed(sc, kbState, ctx.keybinds->hotbar[i])) {
				ctx.hotbar->SelectSlot(i);
				break;
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
				if(hit.hit) { ctx.grid->RemoveBlock(hit.blockPos); }
			} else if(event.button.button == SDL_BUTTON_MIDDLE) {
				if(hit.hit) { ctx.hotbar->SetSlot(ctx.hotbar->SelectedSlot(), hit.blockID); }
			} else if(event.button.button == SDL_BUTTON_RIGHT) {
				if(hit.hit && hit.faceIndex >= 0) {
					const glm::ivec3 placePos = hit.blockPos + kFaceOffset[hit.faceIndex];
					if(!ctx.grid->HasBlockAt(placePos)) {
						const uint32_t selectedBlockID = ctx.hotbar->CurrentBlockID();
						if(selectedBlockID != 0u || ctx.hotbar->SlotHasBlock(ctx.hotbar->SelectedSlot())) {
							if(ctx.physics->CanPlaceBlockAt(*ctx.player, *ctx.camera, placePos)) {
								ctx.grid->AddBlock(placePos.x, placePos.y, placePos.z, selectedBlockID);
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
	// Apply accumulated mouse deltas to camera orientation
	if(ctx.gameState == GameState::PLAYING) {
		ctx.camera->UpdateFromMouseDelta(mouseDeltaX, mouseDeltaY);
	}
	mouseDeltaX = 0.0f;
	mouseDeltaY = 0.0f;

	// Crawl-toggle edge detection
	const bool* const keys       = SDL_GetKeyboardState(nullptr);
	const bool        crawlCombo = ChordHeld(keys, ctx.keybinds->crawl_toggle);
	crawlToggleThisFrame         = crawlCombo && !prevCrawlComboDown;
	prevCrawlComboDown           = crawlCombo;

	if(ctx.gameState == GameState::PLAYING) {
		// Build flat movement direction from camera yaw (no pitch contribution)
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
		const bool crouchHeld     = ChordHeld(keys, ctx.keybinds->crouch);

		ctx.physics->StepEntityEuler(
			*ctx.player,
			static_cast<float>(dt),
			desiredHV,
			ChordHeld(keys, ctx.keybinds->jump),
			crouchHeld,
			crawlToggleThisFrame,
			*ctx.physicsConstants
		);
		ctx.physics->StepBlockGravity(static_cast<float>(dt));
		ctx.physics->UpdateFallingBlocks(static_cast<float>(dt));
		ctx.physics->ForceEntityUpIfInsideBlock(*ctx.player);

		ctx.camera->position = ctx.player->position;
	}

	// 3D rendering
	const float aspect       = (winH > 0) ? static_cast<float>(winW) / static_cast<float>(winH) : 1.0f;
	const glm::mat4 proj     = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
	const glm::mat4 view     = ctx.camera->View();

	if(!(debugView && debugWireframeOnly)) {
		ctx.grid->Draw(*ctx.defaultShader, *ctx.blockAtlas, proj, view);

		std::vector<Grid::FloatBlock> fallingVisual;
		for(const Physics::FallingBlock& fb : ctx.physics->GetFallingBlocks()) {
			fallingVisual.push_back({fb.pos, fb.blockID});
		}
		ctx.grid->DrawFloatBlocks(fallingVisual, *ctx.defaultShader, *ctx.blockAtlas, proj, view);
	}

	// ImGui overlay
	ctx.debugOverlay->NewFrame();

	bool returnedToMenu = false;

	if(ctx.gameState == GameState::PLAYING) {
		if(debugView) {
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
			ImGui::Text("Seed: %d", ctx.currentSeed);

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
		ctx.hotbar->Draw(*ctx.blockRegistry, winW, winH);

	} else if(ctx.gameState == GameState::PAUSE_MENU) {
		bool wantResume   = false;
		bool wantSaveQuit = false;
		DrawPauseMenu(wantSaveQuit, wantResume, winW, winH);

		if(wantResume) {
			ctx.gameState = GameState::PLAYING;
			SDL_SetWindowRelativeMouseMode(ctx.window, true);
		}
		if(wantSaveQuit) {
			WorldFile::Header wfh;
			wfh.seed = ctx.currentSeed;
			std::filesystem::create_directories(ctx.worldsDir);
			if(!WorldFile::Save(ctx.worldSavePath, wfh, *ctx.grid)) {
				std::fprintf(stderr, "Warning: world save failed for '%s'\n", ctx.worldSavePath.c_str());
			}
			ctx.grid->Clear();
			ctx.gameState  = GameState::MAIN_MENU;
			returnedToMenu = true;
		}
	}

	ctx.debugOverlay->Render();
	return returnedToMenu;
}
