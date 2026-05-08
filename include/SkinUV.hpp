#pragma once

#include <array>
#include <cstddef>

namespace SkinUV {
    enum class Face : std::size_t {
        Front = 0,
        Back,
        Left,
        Right,
        Top,
        Bottom,
    };

    // UV rectangle in normalized texture coordinates.
    // The skin texture is loaded flipped vertically
    struct Rect {
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
    };

    struct Cube {
        std::array<Rect, 6> faces{};
        bool mirrored = false;
    };

    struct Tables {
        Cube head;
        Cube body;
        Cube rightArm;
        Cube leftArm;
        Cube rightLeg;
        Cube leftLeg;
    };

    struct OverlayTables {
        Cube hat;
        Cube jacket;
        Cube rightSleeve;
        Cube leftSleeve;
        Cube rightPants;
        Cube leftPants;
    };

    constexpr float U(float px) {
        return px / 64.0f;
    }

    constexpr float V(float py) {
        return 1.0f - (py / 64.0f);
    }

    constexpr Rect MakeRect(float x0, float y0, float x1, float y1) {
        return {U(x0), V(y0), U(x1), V(y1)};
    }

    constexpr Cube MakeCube(float texU, float texV, float width, float height, float depth, bool mirrored = false) {
        const float left      = texU;
        const float front     = texU + depth;
        const float right     = texU + depth + width;
        const float back      = texU + depth + width + depth;
        const float top       = texV;
        const float faceTop   = texV + depth;
        const float faceBottom = texV + depth + height;

        Cube cube{};
        cube.mirrored = mirrored;

        cube.faces[static_cast<std::size_t>(Face::Front)]  = MakeRect(front, faceTop, front + width, faceBottom);
        cube.faces[static_cast<std::size_t>(Face::Back)]   = MakeRect(back,  faceTop, back + width,  faceBottom);
        cube.faces[static_cast<std::size_t>(Face::Left)]   = MakeRect(left,  faceTop, left + depth,  faceBottom);
        cube.faces[static_cast<std::size_t>(Face::Right)]  = MakeRect(right, faceTop, right + depth, faceBottom);
        cube.faces[static_cast<std::size_t>(Face::Top)]    = MakeRect(front, top,    front + width,  faceTop);
        cube.faces[static_cast<std::size_t>(Face::Bottom)] = MakeRect(right, top,    right + width,  faceTop);

        return cube;
    }

    constexpr Tables Minecraft64() {
        return Tables{
            /* head */     MakeCube(0.0f,  0.0f,  8.0f, 8.0f, 8.0f, false),
            /* body */     MakeCube(16.0f, 16.0f, 8.0f, 12.0f, 4.0f, false),
            /* rightArm */ MakeCube(40.0f, 16.0f, 4.0f, 12.0f, 4.0f, false),
            /* leftArm */  MakeCube(40.0f, 16.0f, 4.0f, 12.0f, 4.0f, true),
            /* rightLeg */ MakeCube(0.0f,  16.0f, 4.0f, 12.0f, 4.0f, false),
            /* leftLeg */  MakeCube(0.0f,  16.0f, 4.0f, 12.0f, 4.0f, true),
        };
    }

    constexpr OverlayTables Minecraft64Overlay() {
        return OverlayTables{
            /* hat */        MakeCube(32.0f, 0.0f,  8.0f, 8.0f, 8.0f, false),
            /* jacket */     MakeCube(16.0f, 32.0f, 8.0f, 12.0f, 4.0f, false),
            /* rightSleeve */MakeCube(40.0f, 32.0f, 4.0f, 12.0f, 4.0f, false),
            /* leftSleeve */ MakeCube(48.0f, 48.0f, 4.0f, 12.0f, 4.0f, true),
            /* rightPants */ MakeCube(0.0f,  32.0f, 4.0f, 12.0f, 4.0f, false),
            /* leftPants */  MakeCube(0.0f,  48.0f, 4.0f, 12.0f, 4.0f, true),
        };
    }
}