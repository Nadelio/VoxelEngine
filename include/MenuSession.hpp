#pragma once

#include <vector>
#include <string>

#include "AppContext.hpp"
#include "GameUI.hpp"
#include "Keybinds.hpp"

struct WorldSession;

enum class SettingsPage { MAIN, CONTROLS };

// Handles all frame logic for the menu game states:
//   MAIN_MENU, WORLDS_MENU, NEW_WORLD_MENU, SETTINGS_MENU
// Owns the persistent state shared across menu frames (world list, selection,
// new-world parameters). Calls WorldSession::Enter when the player starts a world.
struct MenuSession {
	std::vector<WorldEntry> worldEntries;
	int                     selectedIdx   = -1;
	NewWorldParams          newWorldParams;

	// Settings state
	SettingsPage settingsPage        = SettingsPage::MAIN;
	int          listeningKeybindIdx = -1;
	Keybinds     editedKeybinds;
	GameState    settingsReturnState = GameState::MAIN_MENU;

	// Draw the current menu and process any state transitions.
	// Returns true if the user pressed Quit (app should exit).
	bool Frame(int winW, int winH, AppContext& ctx, WorldSession& worldSession);
};
