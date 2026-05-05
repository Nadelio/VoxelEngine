#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "WorldFile.hpp"

enum class GameState {
    MAIN_MENU,
    WORLDS_MENU,
    NEW_WORLD_MENU,
    SETTINGS_MENU,
    PAUSE_MENU,
    PLAYING,
};

struct WorldEntry {
    // path to the .world file
    std::string          path;
    // world name is the name of the .world file
    std::string          name;
    // header data from .world file
    WorldFile::Header    header;
};

inline std::vector<WorldEntry> ScanWorlds(const std::string& worldsDir) {
    std::vector<WorldEntry> result;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(worldsDir, ec)) {
        if (entry.path().extension() != ".world") continue;
        WorldEntry we;
        we.path = entry.path().string();
        we.name = entry.path().stem().string();
        WorldFile::ReadHeader(we.path, we.header);
        result.push_back(std::move(we));
    }
    std::sort(result.begin(), result.end(),
              [](const WorldEntry& a, const WorldEntry& b){ return a.name < b.name; });
    return result;
}

struct NewWorldParams {
    char     seedBuf[32]  = {};   // text input; empty = random
    int      worldTypeIdx = 0;    // 0=Default, 1=SingleBiome, 2=Superflat
    char     biomeBuf[64] = {};   // used when worldTypeIdx == 1

    // Resolve to a WorldFile::Header (seed 0 means "randomise at generation time")
    WorldFile::Header MakeHeader() const {
        WorldFile::Header h;
        h.seed      = seedBuf[0] ? std::atoi(seedBuf) : 0;
        h.worldType = static_cast<WorldFile::WorldType>(worldTypeIdx);
        if (h.worldType == WorldFile::WorldType::SingleBiome) h.singleBiome = biomeBuf;
        return h;
    }
};

// Returns true when the user has chosen an action; sets `next` to the
// destination state.  `wantQuit` is set true if the user pressed Quit.
inline bool DrawMainMenu(GameState& next, bool& wantQuit, int winW, int winH) {
    bool acted = false;
    const float w = std::max(240.0f, winW * 0.22f);
    ImGui::SetNextWindowSize(ImVec2(w, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##main_menu", nullptr,
        ImGuiWindowFlags_NoDecoration   |
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_AlwaysAutoResize);

    const float btnW = ImGui::GetContentRegionAvail().x;

    ImGui::SetCursorPosX((btnW - ImGui::CalcTextSize("Voxel Engine").x) * 0.5f + ImGui::GetStyle().WindowPadding.x);
    ImGui::Text("Voxel Engine");
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Worlds", ImVec2(btnW, 0))) {
        next = GameState::WORLDS_MENU;
        acted = true;
    }
    ImGui::Spacing();
    if (ImGui::Button("Settings", ImVec2(btnW, 0))) {
        next = GameState::SETTINGS_MENU;
        acted = true;
    }
    ImGui::Spacing();
    if (ImGui::Button("Quit", ImVec2(btnW, 0))) {
        wantQuit = true;
        acted = true;
    }

    ImGui::End();
    return acted;
}

// `worldsDir`     – base path where .world files live
// `selectedIndex` – persistent selection index (caller owns it, init to -1)
// `wantLoad`      – set true when user double-clicks / presses Load
// `loadPath`      – set to the chosen world path when wantLoad is true
// `wantNew`       – set true when user presses New World
// `wantBack`      – set true when user presses Back
// `entries`       – world list (refreshed here when empty or on Refresh click)
inline void DrawWorldsMenu(const std::string& worldsDir, std::vector<WorldEntry>& entries, int& selectedIndex, bool& wantLoad, std::string& loadPath, std::string& loadName, WorldFile::Header& loadHeader, bool& wantNew, bool& wantDelete, bool& wantBack, int winW, int winH)
{
    if (entries.empty()) entries = ScanWorlds(worldsDir);

    const float w = std::max(360.0f, winW * 0.5f);
    const float h = std::max(300.0f, winH * 0.6f);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##worlds_menu", nullptr,
        ImGuiWindowFlags_NoDecoration   |
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoMove);

    ImGui::Text("Worlds");
    ImGui::Separator();
    ImGui::Spacing();

    const float listHeight = h - 120.0f;
    ImGui::BeginChild("##world_list", ImVec2(0, listHeight), true);
    if (entries.empty()) {
        ImGui::TextDisabled("(no worlds found)");
    }
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        const WorldEntry& we = entries[i];
        const char* typeStr = "Default";
        if (we.header.worldType == WorldFile::WorldType::SingleBiome) typeStr = "Single Biome";
        else if (we.header.worldType == WorldFile::WorldType::Superflat) typeStr = "Superflat";

        char label[256];
        std::snprintf(label, sizeof(label), "%s  [%s, seed %d]##%d",
                      we.name.c_str(), typeStr, we.header.seed, i);

        if (ImGui::Selectable(label, selectedIndex == i,
                              ImGuiSelectableFlags_AllowDoubleClick)) {
            selectedIndex = i;
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                wantLoad = true;
                loadPath = we.path;
                loadName = we.name;
                loadHeader = we.header;
            }
        }
    }
    ImGui::EndChild();

    ImGui::Spacing();

    const float btnW = 110.0f;
    if (ImGui::Button("Load", ImVec2(btnW, 0))) {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            wantLoad = true;
            loadPath = entries[selectedIndex].path;
            loadName = entries[selectedIndex].name;
            loadHeader = entries[selectedIndex].header;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("New World", ImVec2(btnW, 0))) { wantNew = true; }
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(btnW, 0))) {
        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
            wantDelete = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(btnW, 0))) {
        entries = ScanWorlds(worldsDir);
        selectedIndex = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(btnW, 0))) { wantBack = true; }

    ImGui::End();
}

inline void DrawNewWorldMenu(NewWorldParams& params, bool& wantCreate, bool& wantBack, int winW, int winH)
{
    static const char* kWorldTypes[] = {"Default", "Single Biome", "Superflat"};

    const float w = std::max(280.0f, winW * 0.28f);
    ImGui::SetNextWindowSize(ImVec2(w, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##new_world_menu", nullptr,
        ImGuiWindowFlags_NoDecoration   |
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("New World");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Seed (leave blank for random):");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##seed", params.seedBuf, sizeof(params.seedBuf),
                     ImGuiInputTextFlags_CharsDecimal);

    ImGui::Spacing();
    ImGui::Text("World Type:");
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##worldtype", &params.worldTypeIdx, kWorldTypes, 3);

    if (params.worldTypeIdx == 1) {
        ImGui::Spacing();
        ImGui::Text("Biome name:");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("##biome", params.biomeBuf, sizeof(params.biomeBuf));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Create", ImVec2(btnW, 0))) { wantCreate = true; }
    ImGui::SameLine();
    if (ImGui::Button("Back", ImVec2(btnW, 0))) { wantBack = true; }

    ImGui::End();
}

// `wantSaveQuit` – save the world and return to main menu
// `wantResume`   – close menu and resume playing
inline void DrawSettingsMenu(bool& wantBack, int winW, int winH)
{
    const float w = std::max(240.0f, winW * 0.28f);
    ImGui::SetNextWindowSize(ImVec2(w, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##settings_menu", nullptr,
        ImGuiWindowFlags_NoDecoration   |
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_AlwaysAutoResize);

    const float btnW = ImGui::GetContentRegionAvail().x;

    ImGui::Text("Settings");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Controls  (coming soon)");
    ImGui::TextDisabled("Texture Pack  (coming soon)");
    ImGui::TextDisabled("Resource Pack  (coming soon)");
    ImGui::TextDisabled("Data Pack  (coming soon)");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Back", ImVec2(btnW, 0))) { wantBack = true; }

    ImGui::End();
}

inline void DrawPauseMenu(bool& wantSaveQuit, bool& wantResume, int winW, int winH)
{
    const float w = std::max(220.0f, winW * 0.22f);
    ImGui::SetNextWindowSize(ImVec2(w, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##pause_menu", nullptr,
        ImGuiWindowFlags_NoDecoration   |
        ImGuiWindowFlags_NoSavedSettings|
        ImGuiWindowFlags_NoMove         |
        ImGuiWindowFlags_AlwaysAutoResize);

    const float btnW = ImGui::GetContentRegionAvail().x;

    ImGui::Text("Paused");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Resume", ImVec2(btnW, 0))) { wantResume = true; }
    ImGui::Spacing();
    if (ImGui::Button("Save and Quit", ImVec2(btnW, 0))) { wantSaveQuit = true; }
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled("Controls  (placeholder)");
    ImGui::TextDisabled("Texture Pack  (placeholder)");
    ImGui::TextDisabled("Resource Pack  (placeholder)");
    ImGui::TextDisabled("Data Pack  (placeholder)");

    ImGui::End();
}
