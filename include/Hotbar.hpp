#pragma once

#include <array>
#include <cstdint>

#include "BlockRegistry.hpp"
#include "CubeMesh.hpp"

class Hotbar {
public:
    static constexpr int kSlotCount = 9;

    Hotbar() = default;
    ~Hotbar() = default;

    Hotbar(const Hotbar&) = delete;
    Hotbar& operator=(const Hotbar&) = delete;

    void SetSlot(int slot, uint32_t blockID);
    void ClearSlot(int slot);
    bool SlotHasBlock(int slot) const;

    void SelectSlot(int slot);
    void SelectNext();
    void SelectPrev();
    int SelectedSlot() const { return selectedSlot_; }

    uint32_t CurrentBlockID() const;

    // Renders the hotbar as an ImGui window.
    // Must be called between DebugOverlay::NewFrame() and DebugOverlay::Render().
    void Draw(const BlockRegistry& registry, int viewportW, int viewportH) const;

private:
    struct Slot {
        bool     hasBlock = false;
        uint32_t blockID  = 0;
    };

    std::array<Slot, kSlotCount> slots_{};
    int selectedSlot_ = 0;
};
