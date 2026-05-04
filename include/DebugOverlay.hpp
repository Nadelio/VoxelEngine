#pragma once

#include <SDL3/SDL.h>

// Manages the Dear ImGui lifecycle (init, per-frame, shutdown).
// Build the debug window directly with ImGui:: calls between NewFrame() and Render().
class DebugOverlay {
public:
    DebugOverlay() = default;
    ~DebugOverlay();

    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;

    // Initialize ImGui with the SDL3 + OpenGL3 backends.
    bool Initialize(SDL_Window* window, SDL_GLContext glContext);

    // Forward an SDL event to ImGui. Call for every event in the poll loop.
    void ProcessEvent(const SDL_Event& event);

    // Begin a new ImGui frame. Call once per frame after all SDL events are consumed.
    void NewFrame();

    // Submit ImGui draw data to OpenGL. Call once per frame before SDL_GL_SwapWindow.
    void Render();

private:
    bool initialized_ = false;
};