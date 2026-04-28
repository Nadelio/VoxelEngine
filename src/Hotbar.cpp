#include "Hotbar.hpp"

#include <vector>

#include <SDL3/SDL.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    PFNGLGENVERTEXARRAYSPROC  hglGenVertexArrays      = nullptr;
    PFNGLBINDVERTEXARRAYPROC  hglBindVertexArray      = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC hglDeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC       hglGenBuffers           = nullptr;
    PFNGLBINDBUFFERPROC       hglBindBuffer           = nullptr;
    PFNGLBUFFERDATAPROC       hglBufferData           = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC     hglVertexAttribPointer    = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC hglEnableVertexAttribArray = nullptr;
    PFNGLDELETEBUFFERSPROC    hglDeleteBuffers         = nullptr;
    bool gHotbarGlLoaded = false;

    void AppendColoredQuad(std::vector<float>& v, float x, float y, float w, float h) {
        float x1 = x + w, y1 = y + h;
        v.insert(v.end(), {
            x,  y,  0.f, 0.f,    x1, y,  0.f, 0.f,    x1, y1, 0.f, 0.f,
            x,  y,  0.f, 0.f,    x1, y1, 0.f, 0.f,    x,  y1, 0.f, 0.f,
        });
    }

    void AppendTexturedQuad(std::vector<float>& v, float x, float y, float w, float h,
                            const std::array<float, 8>& uv) {
        float x1 = x + w, y1 = y + h;
        float tl_u = uv[6], tl_v = uv[7];
        float tr_u = uv[4], tr_v = uv[5];
        float br_u = uv[2], br_v = uv[3];
        float bl_u = uv[0], bl_v = uv[1];
        v.insert(v.end(), {
            x,  y,  tl_u, tl_v,    x1, y,  tr_u, tr_v,    x1, y1, br_u, br_v,
            x,  y,  tl_u, tl_v,    x1, y1, br_u, br_v,    x,  y1, bl_u, bl_v,
        });
    }
}

Hotbar::~Hotbar() {
    if (hglDeleteBuffers && vbo_ != 0)        { hglDeleteBuffers(1, &vbo_); }
    if (hglDeleteVertexArrays && vao_ != 0)   { hglDeleteVertexArrays(1, &vao_); }
}

bool Hotbar::LoadGLFunctions() {
    if (gHotbarGlLoaded) return true;

    hglGenVertexArrays       = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>    (SDL_GL_GetProcAddress("glGenVertexArrays"));
    hglBindVertexArray       = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>    (SDL_GL_GetProcAddress("glBindVertexArray"));
    hglDeleteVertexArrays    = reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC> (SDL_GL_GetProcAddress("glDeleteVertexArrays"));
    hglGenBuffers            = reinterpret_cast<PFNGLGENBUFFERSPROC>         (SDL_GL_GetProcAddress("glGenBuffers"));
    hglBindBuffer            = reinterpret_cast<PFNGLBINDBUFFERPROC>         (SDL_GL_GetProcAddress("glBindBuffer"));
    hglBufferData            = reinterpret_cast<PFNGLBUFFERDATAPROC>         (SDL_GL_GetProcAddress("glBufferData"));
    hglVertexAttribPointer   = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(SDL_GL_GetProcAddress("glVertexAttribPointer"));
    hglEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
    hglDeleteBuffers         = reinterpret_cast<PFNGLDELETEBUFFERSPROC>      (SDL_GL_GetProcAddress("glDeleteBuffers"));

    gHotbarGlLoaded = hglGenVertexArrays && hglBindVertexArray && hglDeleteVertexArrays &&
                      hglGenBuffers && hglBindBuffer && hglBufferData &&
                      hglVertexAttribPointer && hglEnableVertexAttribArray && hglDeleteBuffers;
    return gHotbarGlLoaded;
}

bool Hotbar::Initialize(const std::string& vertexShaderPath,
                        const std::string& fragmentShaderPath) {
    if (!LoadGLFunctions()) return false;
    if (!shader_.LoadFromFiles(vertexShaderPath, fragmentShaderPath)) return false;

    hglGenVertexArrays(1, &vao_);
    hglGenBuffers(1, &vbo_);

    hglBindVertexArray(vao_);
    hglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    hglBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    hglVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE,
                           4 * static_cast<GLsizei>(sizeof(float)), reinterpret_cast<void*>(0));
    hglEnableVertexAttribArray(0);

    hglVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
                           4 * static_cast<GLsizei>(sizeof(float)),
                           reinterpret_cast<void*>(2 * sizeof(float)));
    hglEnableVertexAttribArray(1);

    hglBindVertexArray(0);
    return true;
}

void Hotbar::SetSlot(int slot, const FaceTileMap& tiles) {
    if (slot < 0 || slot >= kSlotCount) return;
    slots_[slot].hasBlock = true;
    slots_[slot].tiles    = tiles;
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

const FaceTileMap& Hotbar::CurrentTiles() const {
    return slots_[selectedSlot_].tiles;
}

void Hotbar::Draw(const AtlasTexture& atlas, int viewportW, int viewportH) const {
    if (vao_ == 0) return;

    constexpr float kSlotSize   = 50.0f;
    constexpr float kSlotPad    = 4.0f;
    constexpr float kMarginBot  = 12.0f;
    constexpr float kBorderW    = 3.0f;
    constexpr float kIconInset  = 4.0f;

    const float totalW  = kSlotCount * kSlotSize + (kSlotCount - 1) * kSlotPad;
    const float barX    = (static_cast<float>(viewportW) - totalW) * 0.5f;
    const float barY    = static_cast<float>(viewportH) - kMarginBot - kSlotSize;

    const glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(viewportW),
                                             static_cast<float>(viewportH), 0.0f);

    const GLboolean wasDepthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasCullEnabled  = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader_.Use();
    shader_.SetMat4("uMVP", glm::value_ptr(projection));
    shader_.SetInt("uAtlas", 0);

    hglBindVertexArray(vao_);
    hglBindBuffer(GL_ARRAY_BUFFER, vbo_);

    {
        std::vector<float> verts;
        const float bx = barX + selectedSlot_ * (kSlotSize + kSlotPad) - kBorderW;
        const float by = barY - kBorderW;
        const float bw = kSlotSize + kBorderW * 2.0f;
        const float bh = kSlotSize + kBorderW * 2.0f;
        AppendColoredQuad(verts, bx, by, bw, bh);

        shader_.SetVec4("uColor", 1.0f, 1.0f, 1.0f, 0.9f);
        shader_.SetInt("uUseTexture", 0);
        hglBufferData(GL_ARRAY_BUFFER,
                      static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                      verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 4));
    }

    {
        std::vector<float> verts;
        for (int i = 0; i < kSlotCount; ++i) {
            const float sx = barX + i * (kSlotSize + kSlotPad);
            AppendColoredQuad(verts, sx, barY, kSlotSize, kSlotSize);
        }
        shader_.SetVec4("uColor", 0.12f, 0.12f, 0.12f, 0.78f);
        shader_.SetInt("uUseTexture", 0);
        hglBufferData(GL_ARRAY_BUFFER,
                      static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                      verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 4));
    }

    atlas.Bind(GL_TEXTURE0);
    shader_.SetVec4("uColor", 1.0f, 1.0f, 1.0f, 1.0f);
    shader_.SetInt("uUseTexture", 1);

    for (int i = 0; i < kSlotCount; ++i) {
        if (!slots_[i].hasBlock) continue;

        const FaceTile& topTile = slots_[i].tiles[static_cast<int>(CubeFace::Top)];
        const std::array<float, 8> uv = atlas.TileUV32(topTile.x, topTile.y);

        const float ix = barX + i * (kSlotSize + kSlotPad) + kIconInset;
        const float iy = barY + kIconInset;
        const float iSize = kSlotSize - kIconInset * 2.0f;

        std::vector<float> verts;
        AppendTexturedQuad(verts, ix, iy, iSize, iSize, uv);

        hglBufferData(GL_ARRAY_BUFFER,
                      static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                      verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 4));
    }

    hglBindBuffer(GL_ARRAY_BUFFER, 0);
    hglBindVertexArray(0);

    glDisable(GL_BLEND);
    if (wasDepthEnabled) glEnable(GL_DEPTH_TEST);
    if (wasCullEnabled)  glEnable(GL_CULL_FACE);
}
