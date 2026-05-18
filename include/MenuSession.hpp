#pragma once

#include <vector>
#include <string>

#include <SDL3/SDL_opengl.h>

#include "AppContext.hpp"
#include "GameUI.hpp"
#include "Keybinds.hpp"
#include "PlayerModel.hpp"
#include "Shader.hpp"

struct WorldSession;

enum class SettingsPage { MAIN, CONTROLS };

// Handles all frame logic for the menu game states:
//   MAIN_MENU, WORLDS_MENU, NEW_WORLD_MENU, SETTINGS_MENU
// Owns the persistent state shared across menu frames (world list, selection,
// new-world parameters). Calls WorldSession::Enter when the player starts a world.
struct MenuSession {
	~MenuSession();

	std::vector<WorldEntry> worldEntries;
	int                     selectedIdx   = -1;
	NewWorldParams          newWorldParams;
	std::vector<std::string> skinPaths;
	std::vector<std::string> skinNames;
	int                      selectedSkinIdx = -1;

	// Settings state
	SettingsPage settingsPage        = SettingsPage::MAIN;
	int          listeningKeybindIdx = -1;
	Keybinds     editedKeybinds;
	GameState    settingsReturnState = GameState::MAIN_MENU;

	// Draw the current menu and process any state transitions.
	// Returns true if the user pressed Quit (app should exit).
	bool Frame(int winW, int winH, AppContext& ctx, WorldSession& worldSession);

private:
	void EnsureSkinList(AppContext& ctx);
	bool EnsureSkinPreviewResources(AppContext& ctx);
	void RenderSkinPreview(int winW, int winH, AppContext& ctx);
	void ApplySelectedSkin(AppContext& ctx);
	void DestroySkinPreviewResources();

	PlayerModel skinPreviewModel_;
	Shader      skinPreviewShader_;
	GLuint      skinPreviewFbo_ = 0;
	GLuint      skinPreviewColorTex_ = 0;
	GLuint      skinPreviewDepthRbo_ = 0;
	GLuint      skinPreviewShadowTex_ = 0;
	int         skinPreviewSize_ = 384;
	float       skinPreviewYaw_ = 0.0f;
	bool        skinPreviewReady_ = false;
	bool        skinListLoaded_ = false;
	std::string loadedPreviewSkinPath_;
};
