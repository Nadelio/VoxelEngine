#pragma once

#include <array>

#include <SDL3/SDL_opengl.h>

#include "AtlasTexture.hpp"

enum class CubeFace : int {
    Front = 0,
    Back = 1,
    Left = 2,
    Right = 3,
    Top = 4,
    Bottom = 5,
};

struct FaceTile {
    int x = 0;
    int y = 0;
};

using FaceTileMap = std::array<FaceTile, 6>;

class CubeMesh {
public:
    CubeMesh() = default;
    ~CubeMesh();

    CubeMesh(const CubeMesh&) = delete;
    CubeMesh& operator=(const CubeMesh&) = delete;

    bool Initialize();
    void SetFaceTiles(const AtlasTexture& atlas, const FaceTileMap& faceTiles);

    // faceMask: bits 0-5 correspond to CubeFace enum values. A set bit means that face is drawn.
    // Default is 0x3F (all six faces visible).
    void SetVisibleFaces(uint8_t faceMask);
    void Draw() const;

private:
    bool LoadMeshGLFunctions();

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    std::array<float, 24 * 5> vertices_{};
    uint8_t visibleFaces_ = 0x3Fu;
};
