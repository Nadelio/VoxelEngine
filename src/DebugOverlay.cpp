#include "DebugOverlay.hpp"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

DebugOverlay::~DebugOverlay() {
    if (!initialized_) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

bool DebugOverlay::Initialize(SDL_Window* window, SDL_GLContext glContext) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplSDL3_InitForOpenGL(window, glContext)) {
        ImGui::DestroyContext();
        return false;
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330 core")) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    initialized_ = true;
    return true;
}

void DebugOverlay::ProcessEvent(const SDL_Event& event) {
    if (initialized_) {
        ImGui_ImplSDL3_ProcessEvent(&event);
    }
}

void DebugOverlay::NewFrame() {
    if (!initialized_) return;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void DebugOverlay::Render() {
    if (!initialized_) return;
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}