#pragma once

#include <array>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

class AtlasTexture {
public:
    AtlasTexture() = default;
    ~AtlasTexture();

    AtlasTexture(const AtlasTexture&) = delete;
    AtlasTexture& operator=(const AtlasTexture&) = delete;

    bool LoadFromFile(const std::string& atlasPath);
    void Bind(GLenum textureUnit = GL_TEXTURE0) const;

    int    Width()     const { return width_;   }
    int    Height()    const { return height_;  }
    GLuint TextureID() const { return texture_; }

    // Returns UVs in order: bl, br, tr, tl.
    std::array<float, 8> TileUV32(int tileX, int tileY) const;

private:
    GLuint texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};
