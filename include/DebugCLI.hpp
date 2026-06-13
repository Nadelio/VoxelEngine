#pragma once

// Forward declaration
struct AppContext;

// Simple in-game debug command-line interface.
// Opens with the `open_cli` keybind (default: /), accepts a single command,
// shows a one-line result, and closes on Escape.
//
// IDs may be numeric (e.g. 3) or a registered name (e.g. grass).
// Coordinates may be written with or without parentheses; commas are optional separators.
struct DebugCLI {
	bool isOpen = false;

	// Open the CLI: show the input bar and release mouse capture.
	void Open(AppContext& ctx);

	// Close the CLI: hide the input bar and restore mouse capture.
	void Close(AppContext& ctx);

	// Render the CLI ImGui window.
	// Must be called between DebugOverlay::NewFrame() and DebugOverlay::Render().
	void Draw(int winW, int winH, AppContext& ctx);

private:
	char inputBuf_[256] = {};
	char resultBuf_[512] = {};
	bool hasResult_  = false;
	bool focusNext_  = false;

	void Execute(const char* cmd, AppContext& ctx);
};
