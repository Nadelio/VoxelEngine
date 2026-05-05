#pragma once

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "BiomeRegistry.hpp"
#include "FileDialog.hpp"
#include "Keybinds.hpp"
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
    int      biomeIdx     = 0;    // index into BiomeRegistry::Biomes(), used when worldTypeIdx == 1

    std::vector<SuperflatLayer> superflatLayers = {
        {2u, 4}, // stone x 4  (bottom)
        {1u, 2}, // dirt  x 2
        {0u, 1}, // grass x 1  (top)
    };

    // Data pack folder paths to embed in this world and reload on every play session.
    std::vector<std::string> datapacks;

    WorldFile::Header MakeHeader(const BiomeRegistry* biomes = nullptr) const {
        WorldFile::Header h;
        h.seed      = seedBuf[0] ? std::atoi(seedBuf) : 0;
        h.worldType = static_cast<WorldFile::WorldType>(worldTypeIdx);
        if (h.worldType == WorldFile::WorldType::SingleBiome && biomes) {
            const auto& blist = biomes->Biomes();
            if (biomeIdx >= 0 && biomeIdx < static_cast<int>(blist.size()))
                h.singleBiome = blist[biomeIdx].id;
        }
        if (h.worldType == WorldFile::WorldType::Superflat)
            h.superflatLayers = superflatLayers;
        h.datapacks = datapacks;
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
inline void DrawWorldsMenu(const std::string& worldsDir, std::vector<WorldEntry>& entries, int& selectedIndex, bool& wantLoad, std::string& loadPath, std::string& loadName, WorldFile::Header& loadHeader, bool& wantNew, bool& wantDelete, bool& wantRename, std::string& renameNewName, bool& wantBack, int winW, int winH)
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

    const float listHeight = h - 170.0f;
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

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float btnW    = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;

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
    {
        static char renameBuf[256] = {};
        if (ImGui::Button("Rename World", ImVec2(btnW, 0))) {
            if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size())) {
                std::snprintf(renameBuf, sizeof(renameBuf), "%s", entries[selectedIndex].name.c_str());
                ImGui::OpenPopup("Rename World##popup");
            }
        }
        if (ImGui::BeginPopupModal("Rename World##popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("New name:");
            ImGui::SetNextItemWidth(250.0f);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            const bool confirmed = ImGui::InputText("##rename_input", renameBuf, sizeof(renameBuf),
                                                    ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::Spacing();
            if ((ImGui::Button("Confirm", ImVec2(120, 0)) || confirmed) && renameBuf[0] != '\0') {
                renameNewName = renameBuf;
                wantRename    = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }

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

inline void DrawNewWorldMenu(NewWorldParams& params, bool& wantCreate, bool& wantBack, const BiomeRegistry& biomes, const BlockRegistry& blocks, int winW, int winH)
{
    static const char* kWorldTypes[] = {"Default", "Single Biome", "Superflat"};

    const float w = std::max(params.worldTypeIdx == 2 ? 400.0f : 280.0f,
                             winW * (params.worldTypeIdx == 2 ? 0.38f : 0.28f));
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
        ImGui::Text("Biome:");
        ImGui::SetNextItemWidth(-1);
        const auto& blist = biomes.Biomes();
        if (!blist.empty()) {
            if (params.biomeIdx >= static_cast<int>(blist.size())) params.biomeIdx = 0;
            std::vector<const char*> names;
            names.reserve(blist.size());
            for (const auto& b : blist) names.push_back(b.displayName.c_str());
            ImGui::Combo("##biome", &params.biomeIdx, names.data(), static_cast<int>(names.size()));
        } else {
            ImGui::TextDisabled("(no biomes loaded)");
        }
    }

    if (params.worldTypeIdx == 2) {
        std::vector<std::pair<uint32_t, std::string>> blockList;
        blockList.reserve(blocks.Blocks().size());
        for (const auto& [id, bd] : blocks.Blocks())
            blockList.push_back({id, bd.name});
        std::sort(blockList.begin(), blockList.end(),
                  [](const auto& a, const auto& b){ return a.first < b.first; });
        std::vector<const char*> bnames;
        bnames.reserve(blockList.size());
        for (const auto& bl : blockList) bnames.push_back(bl.second.c_str());
        auto findBlockIdx = [&](uint32_t id) -> int {
            for (int bi = 0; bi < static_cast<int>(blockList.size()); ++bi)
                if (blockList[bi].first == id) return bi;
            return 0;
        };

        ImGui::Spacing();
        ImGui::SeparatorText("Layers (top = surface)");
        const float sp     = ImGui::GetStyle().ItemSpacing.x;
        const float frmH   = ImGui::GetFrameHeight();
        const float sqW    = frmH;
        const float thickW = 70.0f;
        const float avail  = ImGui::GetContentRegionAvail().x;
        const float comboW = avail - thickW - sp - sqW - sp - sqW - sp - sqW;

        ImGui::BeginChild("##sf_layers", ImVec2(0, 200.0f), ImGuiChildFlags_Borders);

        int deleteIdx = -1, swapIdx = -1, swapDir = 0;
        const int n = static_cast<int>(params.superflatLayers.size());
        for (int di = 0; di < n; ++di) {
            const int i = n - 1 - di;
            auto& layer = params.superflatLayers[i];
            ImGui::PushID(i);

            int blkIdx = findBlockIdx(layer.blockID);
            ImGui::SetNextItemWidth(std::max(comboW, 60.0f));
            if (!bnames.empty() &&
                ImGui::Combo("##blk", &blkIdx, bnames.data(), static_cast<int>(bnames.size())))
                layer.blockID = blockList[blkIdx].first;

            ImGui::SameLine(0, sp);
            ImGui::SetNextItemWidth(thickW);
            ImGui::DragInt("##thick", &layer.thickness, 0.15f, 1, 255, "%d");
            layer.thickness = std::max(1, std::min(255, layer.thickness));

            ImGui::SameLine(0, sp);
            ImGui::BeginDisabled(i >= n - 1);
            if (ImGui::Button("^##up", ImVec2(sqW, 0))) { swapIdx = i; swapDir = 1; }
            ImGui::EndDisabled();

            ImGui::SameLine(0, sp);
            ImGui::BeginDisabled(i <= 0);
            if (ImGui::Button("v##dn", ImVec2(sqW, 0))) { swapIdx = i; swapDir = -1; }
            ImGui::EndDisabled();

            ImGui::SameLine(0, sp);
            if (ImGui::Button("X##del", ImVec2(sqW, 0))) deleteIdx = i;

            ImGui::PopID();
        }
        if (n == 0)
            ImGui::TextDisabled("(no layers - world will be empty)");

        ImGui::EndChild();

        if (deleteIdx >= 0)
            params.superflatLayers.erase(params.superflatLayers.begin() + deleteIdx);
        if (swapIdx >= 0 && swapDir == 1 && swapIdx < n - 1)
            std::swap(params.superflatLayers[swapIdx], params.superflatLayers[swapIdx + 1]);
        if (swapIdx >= 0 && swapDir == -1 && swapIdx > 0)
            std::swap(params.superflatLayers[swapIdx], params.superflatLayers[swapIdx - 1]);

        if (ImGui::Button("+ Add Layer", ImVec2(-1, 0))) {
            const uint32_t defBlock = blockList.empty() ? 2u : blockList[0].first;
            params.superflatLayers.push_back({defBlock, 1});
        }
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Data Packs");
    {
        int dpDel = -1;
        if (!params.datapacks.empty()) {
            const float rowH  = ImGui::GetFrameHeightWithSpacing();
            const float listH = std::min(90.0f, rowH * static_cast<float>(params.datapacks.size()));
            ImGui::BeginChild("##dp_list", ImVec2(0, listH), ImGuiChildFlags_Borders);
            for (int di = 0; di < static_cast<int>(params.datapacks.size()); ++di) {
                ImGui::PushID(di);
                const std::string fname = std::filesystem::path(params.datapacks[di]).filename().string();
                ImGui::TextUnformatted(fname.empty() ? params.datapacks[di].c_str() : fname.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("X")) dpDel = di;
                ImGui::PopID();
            }
            ImGui::EndChild();
        } else {
            ImGui::TextDisabled("(none)");
        }
        if (dpDel >= 0)
            params.datapacks.erase(params.datapacks.begin() + dpDel);
    }
    if (ImGui::Button("+ Add Data Pack", ImVec2(-1, 0))) {
        const std::string picked = OpenNativeFolderDialog("Select custom assets/data folder");
        if (!picked.empty()) params.datapacks.push_back(picked);
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


inline void DrawSettingsMenu(
    bool& wantBack,
    bool& wantOpenControls,
    std::string& pickedBlockAtlasPath,
    std::string& pickedItemAtlasPath,
    std::string& pickedResourcePackPath,
    std::string& pickedDataPackPath,
    int winW, int winH)
{
    const float w = std::max(280.0f, winW * 0.30f);
    ImGui::SetNextWindowSize(ImVec2(w, 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##settings_menu", nullptr,
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove          |
        ImGuiWindowFlags_AlwaysAutoResize);

    const float btnW = ImGui::GetContentRegionAvail().x;

    ImGui::Text("Settings");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::SeparatorText("Controls");
    if (ImGui::Button("Edit Keybinds", ImVec2(btnW, 0)))
        wantOpenControls = true;
    ImGui::Spacing();

    ImGui::SeparatorText("Texture Pack");
    const float halfW = (btnW - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("block_atlas.png##tp_block", ImVec2(halfW, 0)))
        pickedBlockAtlasPath = OpenNativeFileDialog("Select block_atlas.png", "*.png");
    ImGui::SameLine();
    if (ImGui::Button("item_atlas.png##tp_item", ImVec2(halfW, 0)))
        pickedItemAtlasPath = OpenNativeFileDialog("Select item_atlas.png", "*.png");
    ImGui::Spacing();

    ImGui::SeparatorText("Resource Pack");
    if (ImGui::Button("Load custom assets folder##rp", ImVec2(btnW, 0)))
        pickedResourcePackPath = OpenNativeFolderDialog("Select custom assets folder");
    ImGui::Spacing();

    ImGui::SeparatorText("Data Pack");
    if (ImGui::Button("Load custom data folder##dp", ImVec2(btnW, 0)))
        pickedDataPackPath = OpenNativeFolderDialog("Select custom assets/data folder");
    ImGui::Spacing();

    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Back", ImVec2(btnW, 0))) { wantBack = true; }

    ImGui::End();
}

inline void DrawControlsMenu(Keybinds& kb, int& listeningIdx, bool& wantBack, int winW, int winH)
{
    struct KBEntry { const char* label; KeyChord* chord; /* null = section header */ };
    KBEntry entries[] = {
        {"General",          nullptr},
        {"Quit",             &kb.quit},
        {"Pause",            &kb.pause},
        {"Move Forward",     &kb.move_forward},
        {"Move Backward",    &kb.move_back},
        {"Move Left",        &kb.move_left},
        {"Move Right",       &kb.move_right},
        {"Jump",             &kb.jump},
        {"Crouch",           &kb.crouch},
        {"Crawl Toggle",     &kb.crawl_toggle},
        {"Hotbar",           nullptr},
        {"Hotbar 1",         &kb.hotbar[0]},
        {"Hotbar 2",         &kb.hotbar[1]},
        {"Hotbar 3",         &kb.hotbar[2]},
        {"Hotbar 4",         &kb.hotbar[3]},
        {"Hotbar 5",         &kb.hotbar[4]},
        {"Hotbar 6",         &kb.hotbar[5]},
        {"Hotbar 7",         &kb.hotbar[6]},
        {"Hotbar 8",         &kb.hotbar[7]},
        {"Hotbar 9",         &kb.hotbar[8]},
        {"Debug",            nullptr},
        {"Debug Toggle",     &kb.debug_toggle},
        {"Debug Wireframe",  &kb.debug_wireframe},
        {"Debug Block",      &kb.debug_block},
        {"Debug Face",       &kb.debug_face},
        {"Debug Data",       &kb.debug_data},
        {"Debug WF Only",    &kb.debug_wireframe_only},
        {"Debug Stance",     &kb.debug_stance},
        {"Debug Velocity",   &kb.debug_velocity},
        {"Debug Reload",     &kb.debug_reload},
        {"Debug Save",       &kb.debug_save},
        {"Debug Load",       &kb.debug_load},
    };
    const int entryCount = static_cast<int>(sizeof(entries) / sizeof(entries[0]));

    static int listenStartFrame = -1;
    static int prevListeningIdx = -1;
    if (prevListeningIdx != listeningIdx) {
        if (listeningIdx >= 0)
            listenStartFrame = ImGui::GetFrameCount();
        prevListeningIdx = listeningIdx;
    }
    const bool captureActive = (listeningIdx >= 0) &&
                               (ImGui::GetFrameCount() > listenStartFrame);

    if (captureActive) {
        for (int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k) {
            const ImGuiKey key = static_cast<ImGuiKey>(k);
            if (!ImGui::IsKeyPressed(key, /*repeat=*/false)) continue;

            int numKeys = 0;
            const bool* kbState = SDL_GetKeyboardState(&numKeys);
            KeyChord newChord;
            for (int sc = 0; sc < numKeys; ++sc) {
                if (kbState[sc]) {
                    const SDL_Scancode scancode = static_cast<SDL_Scancode>(sc);
                    if (!ScancodeToKeyToken(scancode).empty())
                        newChord.keys.push_back(scancode);
                }
            }

            if (newChord.keys.size() == 1 && newChord.keys[0] == SDL_SCANCODE_ESCAPE) {
                listeningIdx = -1;
            } else if (!newChord.keys.empty() &&
                       listeningIdx < entryCount && entries[listeningIdx].chord) {
                *entries[listeningIdx].chord = std::move(newChord);
                listeningIdx = -1;
            }
            break;
        }
    }

    const float w = std::max(440.0f, winW * 0.46f);
    const float h = std::max(300.0f, winH * 0.72f);
    ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);    ImGui::SetNextWindowPos(
        ImVec2(winW * 0.5f, winH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::Begin("##controls_menu", nullptr,
        ImGuiWindowFlags_NoDecoration    |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove);

    ImGui::Text("Controls");
    if (listeningIdx >= 0) {
        ImGui::SameLine();
        ImGui::TextDisabled("  –  Press keys... (Esc = cancel)");
    }
    ImGui::Separator();
    ImGui::Spacing();

    // Reserve enough height for: Spacing + Separator + Spacing + Back button.
    // Computed from actual scaled ImGui style values so the button is never
    // pushed out of the window regardless of UI scale.
    const float footerReserve =
        ImGui::GetStyle().ItemSpacing.y * 1.5f +   // two Spacing() calls (each adds ItemSpacing.y/2)
        ImGui::GetStyle().SeparatorSize       +
        ImGui::GetFrameHeightWithSpacing();         // button row including trailing spacing
    const float tableH = std::max(50.0f, ImGui::GetContentRegionAvail().y - footerReserve);

    ImGui::BeginChild("##kb_scroll", ImVec2(0.0f, tableH), false);
    if (ImGui::BeginTable("##kb_table", 3,
            ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed,   170.0f);
        ImGui::TableSetupColumn("Key",    ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("##btn",  ImGuiTableColumnFlags_WidthFixed,    80.0f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < entryCount; ++i) {
            const KBEntry& e = entries[i];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            if (!e.chord) {
                ImGui::TextDisabled("%s", e.label);
            } else {
                ImGui::TextUnformatted(e.label);

                ImGui::TableSetColumnIndex(1);
                if (listeningIdx == i)
                    ImGui::TextDisabled("...");
                else
                    ImGui::TextUnformatted(ChordToDisplayString(*e.chord).c_str());

                ImGui::TableSetColumnIndex(2);
                char btnId[32];
                std::snprintf(btnId, sizeof(btnId),
                    (listeningIdx == i) ? "Cancel##%d" : "Rebind##%d", i);
                if (ImGui::Button(btnId, ImVec2(-FLT_MIN, 0.0f))) {
                    if (listeningIdx == i) {
                        listeningIdx = -1;
                    } else {
                        listeningIdx     = i;
                        listenStartFrame = ImGui::GetFrameCount();
                        prevListeningIdx = i;
                    }
                }
            }
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::Button("Back  (saves keybinds)", ImVec2(-FLT_MIN, 0.0f)))
        wantBack = true;

    ImGui::End();
}

inline void DrawPauseMenu(bool& wantSaveQuit, bool& wantResume, bool& wantSettings, int winW, int winH)
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

    if (ImGui::Button("Resume", ImVec2(btnW, 0)))       { wantResume   = true; }
    ImGui::Spacing();
    if (ImGui::Button("Settings", ImVec2(btnW, 0)))     { wantSettings = true; }
    ImGui::Spacing();
    if (ImGui::Button("Save and Quit", ImVec2(btnW, 0))) { wantSaveQuit = true; }

    ImGui::End();
}
