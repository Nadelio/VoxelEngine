#pragma once

#include <string>

#include <SDL3/SDL_opengl.h>

// Loads a player skin PNG into an OpenGL texture
class SkinTexture {
public:
    SkinTexture() = default;
    ~SkinTexture();

    SkinTexture(const SkinTexture&) = delete;
    SkinTexture& operator=(const SkinTexture&) = delete;

    bool LoadFromFile(const std::string& skinPath);
    void Bind(GLenum textureUnit = GL_TEXTURE0) const;

    int    Width()     const { return width_; }
    int    Height()    const { return height_; }
    GLuint TextureID() const { return texture_; }

private:
    static bool loadGLFunctions();

    GLuint texture_ = 0;
    int width_ = 0;
    int height_ = 0;
};