#pragma once

#include <SDL3/SDL.h>

#include "AppContext.hpp"
#include "WorldFile.hpp"

// Handles all frame logic for the PLAYING and PAUSE_MENU game states:
//   - keyboard/mouse input while in-world
//   - physics stepping
//   - 3D world rendering
//   - in-world ImGui overlay (debug window, hotbar, pause menu)
struct WorldSession {
	// Debug-view toggles
	bool debugView          = true;
	bool debugWireframe     = false;
	bool debugLookedAtBlock = false;
	bool debugLookedAtFace  = true;
	bool debugLookedAtData  = true;
	bool debugWireframeOnly = false;
	bool debugStance        = true;
	bool debugVelocity      = true;

	// Crawl-toggle edge-detect state
	bool prevCrawlComboDown   = false;
	bool crawlToggleThisFrame = false;

	// Accumulated mouse motion for the current frame (reset at frame start by caller)
	float mouseDeltaX = 0.0f;
	float mouseDeltaY = 0.0f;

	// Called when transitioning from a menu into gameplay.
	// Sets spawn position, world save path, and enables mouse capture.
	// `savePath` is the file to save to; if empty the seed-based default path is used.
	void Enter(AppContext& ctx, const WorldFile::Header& header, const std::string& savePath = "");

	// Feed one SDL event to the world session. Only acts while PLAYING or PAUSE_MENU.
	void ProcessEvent(const SDL_Event& event, AppContext& ctx);

	// Run one full frame: physics update, 3D render, ImGui overlay (including
	// debugOverlay.NewFrame / Render).
	// Returns true if the player chose "Save and Quit" and we returned to MAIN_MENU.
	bool Frame(double dt, int displayedFps, int winW, int winH, AppContext& ctx);
};
