#include "SkinTexture.hpp"

#include <SDL3/SDL.h>

#include <stb_image.h>

namespace {
    PFNGLACTIVETEXTUREPROC pglActiveTexture = nullptr;

    bool LoadGLFunctions() {
        if (pglActiveTexture) {
            return true;
        }
        pglActiveTexture = reinterpret_cast<PFNGLACTIVETEXTUREPROC>(SDL_GL_GetProcAddress("glActiveTexture"));
        return pglActiveTexture != nullptr;
    }
}

SkinTexture::~SkinTexture() {
    if (texture_ != 0) {
        glDeleteTextures(1, &texture_);
    }
}

bool SkinTexture::loadGLFunctions() {
    return LoadGLFunctions();
}

bool SkinTexture::LoadFromFile(const std::string& skinPath) {
    int loadedWidth = 0;
    int loadedHeight = 0;

    stbi_set_flip_vertically_on_load(1);
    unsigned char* pixels = stbi_load(skinPath.c_str(), &loadedWidth, &loadedHeight, nullptr, STBI_rgb_alpha);
    stbi_set_flip_vertically_on_load(0);

    if (!pixels) {
        SDL_Log("Could not load skin '%s': %s", skinPath.c_str(), stbi_failure_reason());
        return false;
    }

    width_ = loadedWidth;
    height_ = loadedHeight;

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

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);
    return true;
}

void SkinTexture::Bind(GLenum textureUnit) const {
    if (loadGLFunctions()) {
        pglActiveTexture(textureUnit);
    }
    glBindTexture(GL_TEXTURE_2D, texture_);
}