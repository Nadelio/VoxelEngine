#include "Hotbar.hpp"

#include <imgui.h>

void Hotbar::SetSlot(int slot, uint32_t blockID) {
    if (slot < 0 || slot >= kSlotCount) return;
    slots_[slot].hasBlock = true;
    slots_[slot].blockID  = blockID;
}

void Hotbar::ClearSlot(int slot) {
    if (slot < 0 || slot >= kSlotCount) return;
    slots_[slot].hasBlock = false;
}

bool Hotbar::SlotHasBlock(int slot) const {
    if (slot < 0 || slot >= kSlotCount) return false;
    return slots_[slot].hasBlock;
}

void Hotbar::SelectSlot(int slot) {
    if (slot >= 0 && slot < kSlotCount) {
        selectedSlot_ = slot;
    }
}

void Hotbar::SelectNext() {
    selectedSlot_ = (selectedSlot_ + 1) % kSlotCount;
}

void Hotbar::SelectPrev() {
    selectedSlot_ = (selectedSlot_ + kSlotCount - 1) % kSlotCount;
}

uint32_t Hotbar::CurrentBlockID() const {
    if (!slots_[selectedSlot_].hasBlock) return 0;
    return slots_[selectedSlot_].blockID;
}

void Hotbar::Draw(const BlockRegistry& registry, int viewportW, int viewportH) const {
    constexpr float kSlotSize  = 52.0f;
    constexpr float kIconSize  = 40.0f;
    constexpr float kPad       = 4.0f;
    constexpr float kMarginBot = 16.0f;

    const float totalW = kSlotCount * kSlotSize + (kSlotCount - 1) * kPad;
    const float winX   = (static_cast<float>(viewportW) - totalW) * 0.5f;
    const float winY   = static_cast<float>(viewportH) - kMarginBot - kSlotSize;

    ImGui::SetNextWindowPos(ImVec2{winX, winY}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2{totalW, kSlotSize}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.65f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0.0f, 0.0f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2{kPad, 0.0f});
    ImGui::Begin("##hotbar", nullptr,
        ImGuiWindowFlags_NoDecoration       |
        ImGuiWindowFlags_NoSavedSettings    |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav              |
        ImGuiWindowFlags_NoMove             |
        ImGuiWindowFlags_NoInputs);

    for (int i = 0; i < kSlotCount; ++i) {
        if (i > 0) ImGui::SameLine();

        const bool selected = (i == selectedSlot_);
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4{0.9f, 0.8f, 0.1f, 0.85f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.8f, 0.1f, 0.85f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4{0.9f, 0.8f, 0.1f, 0.85f});
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4{0.12f, 0.12f, 0.12f, 0.78f});
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.20f, 0.20f, 0.20f, 0.78f});
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4{0.12f, 0.12f, 0.12f, 0.78f});
        }

        if (slots_[i].hasBlock) {
            const BlockData* blockData = registry.Get(slots_[i].blockID);
            if (blockData && blockData->atlas) {
                const FaceTile& tile = blockData->faceTiles[static_cast<int>(CubeFace::Top)];
                const std::array<float, 8> uv = blockData->atlas->TileUV32(tile.x, tile.y);
                const ImVec2 uv0{uv[6], uv[7]};
                const ImVec2 uv1{uv[2], uv[3]};
                ImGui::ImageButton(("##slot" + std::to_string(i)).c_str(),
                    ImTextureRef(static_cast<ImTextureID>(blockData->atlas->TextureID())),
                    ImVec2{kIconSize, kIconSize}, uv0, uv1);
            } else {
                ImGui::Button(("##slot" + std::to_string(i)).c_str(), ImVec2{kSlotSize, kSlotSize});
            }
        } else {
            ImGui::Button(("##slot" + std::to_string(i)).c_str(), ImVec2{kSlotSize, kSlotSize});
        }

        ImGui::PopStyleColor(3);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

