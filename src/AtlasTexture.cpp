#include "AtlasTexture.hpp"

#include <array>

#include "MeshConstants.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace {
    PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;

    bool LoadAtlasFunctions() {
        if (pglActiveTexture) {
            return true;
        }
        pglActiveTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(SDL_GL_GetProcAddress("glActiveTexture"));
        return pglActiveTexture != nullptr;
    }
}

AtlasTexture::~AtlasTexture() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
    }
}

bool AtlasTexture::LoadFromFile(const std::string& atlasPath) {
    int channels = 0;
    unsigned char* pixels = stbi_load(atlasPath.c_str(), &width_, &height_, &channels, STBI_rgb_alpha);
    if (!pixels) {
        SDL_Log("Could not load atlas '%s': %s", atlasPath.c_str(), stbi_failure_reason());
        return false;
    }

    if (texture_ == 0) {
        glGenTextures(1, &texture_);
    }

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width_,
        height_,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        pixels
    );

    stbi_image_free(pixels);
    return true;
}

void AtlasTexture::Bind(GLenum textureUnit) const {
    if (LoadAtlasFunctions()) {
        pglActiveTexture(textureUnit);
    }
    glBindTexture(GL_TEXTURE_2D, texture_);
}

std::array<float, 8> AtlasTexture::TileUV32(int tileX, int tileY) const {
    if (width_ <= 0 || height_ <= 0) {
        return {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    }

    const float tileW = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(width_);
    const float tileH = static_cast<float>(MeshConstants::kTilePixelSize) / static_cast<float>(height_);

    const float u0 = static_cast<float>(tileX) * tileW;
    const float v0 = static_cast<float>(tileY) * tileH;
    const float u1 = u0 + tileW;
    const float v1 = v0 + tileH;

    return {
        u0, v1,
        u1, v1,
        u1, v0,
        u0, v0,
    };
}
