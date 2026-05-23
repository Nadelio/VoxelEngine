#pragma once

#include <string>

#include <SDL3/SDL.h>

#include "AtlasTexture.hpp"
#include "BiomeRegistry.hpp"
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

struct MenuSession; // forward declaration to avoid circular include

enum class WindowMode {
	WINDOWED = 0,
	WINDOWED_FULLSCREEN = 1,
	FULLSCREEN = 2,
};

// Flat context struct that holds non-owning pointers to all shared engine
// resources and mutable game-wide state. Populated once in main() and then
// passed by reference to the two session handlers.
struct AppContext {
	// Window / rendering
	SDL_Window*       window             = nullptr;
	Shader*           defaultShader      = nullptr;
	Shader*           wireframeShader    = nullptr;
	AtlasTexture*     blockAtlas         = nullptr;
	AtlasTexture*     itemAtlas          = nullptr;

	// Game objects
	BlockRegistry*    blockRegistry      = nullptr;
	Grid*             grid               = nullptr;
	Physics*          physics            = nullptr;
	PhysicsConstants* physicsConstants   = nullptr;
	Camera*           camera             = nullptr;
	Physics::Entity*  player             = nullptr;
	Hotbar*           hotbar             = nullptr;
	Keybinds*         keybinds           = nullptr;
	DebugOverlay*     debugOverlay       = nullptr;
	MenuSession*      menuSession        = nullptr;
	BiomeRegistry*    biomeRegistry      = nullptr;

	// Asset paths (needed for hot-reload and world management)
	std::string worldsDir;
	std::string blocksDataPath;
	std::string physicsConstantsPath;
	std::string keybindsDataPath;
	std::string selectedSkinPath;
	std::string selectedSkinName;
	std::string selectedCapePath;
	std::string selectedCapeName;
	bool        capeEnabled               = true;    bool        toggleSprint              = false;
	// Mutable game-wide state
	GameState         gameState          = GameState::MAIN_MENU;
	WindowMode        windowMode         = WindowMode::WINDOWED_FULLSCREEN;
	int64_t           currentSeed        = 0;
	std::string       worldSavePath;
	WorldFile::Header currentWorldHeader;
	bool              screenshotRequested = false;
	int               windowedWidth = 1280;
	int               windowedHeight = 720;

	// Structure editing session state (set when a .struct file is open for editing)
	bool             isStructureSession = false;
	glm::ivec3       structureOrigin    = {0, 0, 0};
};
