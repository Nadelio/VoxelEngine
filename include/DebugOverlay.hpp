#pragma once

#include <string>
#include <vector>

#include <SDL3/SDL_opengl.h>

#include "Shader.hpp"

class DebugOverlay {
public:
    DebugOverlay() = default;
    ~DebugOverlay();

    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;

    bool Initialize(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
    void DrawFps(int fps, int viewportWidth, int viewportHeight);
    void DrawText(const std::string& text, float x, float y, float scale,
                  int viewportWidth, int viewportHeight);

private:
    static bool LoadOpenGLFunctions();
    static void AppendGlyph(std::vector<float>& vertices, float x, float y, float scale, char glyph);
    static void AppendRect(std::vector<float>& vertices, float x, float y, float width, float height);

    Shader shader_;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
};