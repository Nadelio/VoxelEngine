#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "AtlasTexture.hpp"
#include "BlockRegistry.hpp"
#include "Camera.hpp"
#include "DebugOverlay.hpp"
#include "GameUI.hpp"
#include "Grid.hpp"
#include "Hotbar.hpp"
#include "Keybinds.hpp"
#include "Physics.hpp"
#include "PhysicsConstants.hpp"
#include "Shader.hpp"

// Flat context struct that holds non-owning pointers to all shared engine
// resources and mutable game-wide state. Populated once in main() and then
// passed by reference to the two session handlers.
struct AppContext {
	// Window / rendering
	SDL_Window*       window          = nullptr;
	Shader*           defaultShader   = nullptr;
	Shader*           wireframeShader = nullptr;
	AtlasTexture*     blockAtlas      = nullptr;

	// Game objects
	BlockRegistry*    blockRegistry   = nullptr;
	Grid*             grid            = nullptr;
	Physics*          physics         = nullptr;
	PhysicsConstants* physicsConstants = nullptr;
	Camera*           camera          = nullptr;
	Physics::Entity*  player          = nullptr;
	Hotbar*           hotbar          = nullptr;
	Keybinds*         keybinds        = nullptr;
	DebugOverlay*     debugOverlay    = nullptr;

	// Asset paths (needed for hot-reload and world management)
	std::string worldsDir;
	std::string blocksDataPath;
	std::string physicsConstantsPath;
	std::string keybindsDataPath;

	// Mutable game-wide state
	GameState   gameState    = GameState::MAIN_MENU;
	int         currentSeed  = 0;
	std::string worldSavePath;
};
