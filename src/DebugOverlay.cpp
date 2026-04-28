#include "DebugOverlay.hpp"

#include <array>

#include <SDL3/SDL.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace {
    PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
    PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
    PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
    PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;
    PFNGLBUFFERDATAPROC pglBufferData = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;

    bool gOverlayGlLoaded = false;

    void* GLProc(const char* name) {
        return SDL_GL_GetProcAddress(name);
    }

    std::array<const char*, 5> GlyphRows(char glyph) {
        switch (glyph) {
            case '0': return {"111", "101", "101", "101", "111"};
            case '1': return {"010", "110", "010", "010", "111"};
            case '2': return {"111", "001", "111", "100", "111"};
            case '3': return {"111", "001", "111", "001", "111"};
            case '4': return {"101", "101", "111", "001", "001"};
            case '5': return {"111", "100", "111", "001", "111"};
            case '6': return {"111", "100", "111", "101", "111"};
            case '7': return {"111", "001", "001", "001", "001"};
            case '8': return {"111", "101", "111", "101", "111"};
            case '9': return {"111", "101", "111", "001", "111"};
            case 'A': return {"111", "101", "111", "101", "101"};
            case 'B': return {"110", "101", "110", "101", "110"};
            case 'C': return {"111", "100", "100", "100", "111"};
            case 'D': return {"110", "101", "101", "101", "110"};
            case 'E': return {"111", "100", "111", "100", "111"};
            case 'F': return {"111", "100", "111", "100", "100"};
            case 'G': return {"111", "100", "101", "101", "111"};
            case 'H': return {"101", "101", "111", "101", "101"};
            case 'I': return {"111", "010", "010", "010", "111"};
            case 'J': return {"111", "010", "010", "010", "110"};
            case 'K': return {"101", "101", "110", "101", "101"};
            case 'L': return {"100", "100", "100", "100", "111"};
            case 'M': return {"101", "111", "111", "101", "101"};
            case 'N': return {"101", "111", "111", "111", "101"};
            case 'O': return {"111", "101", "101", "101", "111"};
            case 'P': return {"110", "101", "110", "100", "100"};
            case 'Q': return {"111", "101", "101", "111", "001"};
            case 'R': return {"110", "101", "110", "101", "101"};
            case 'S': return {"111", "100", "111", "001", "111"};
            case 'T': return {"111", "010", "010", "010", "010"};
            case 'U': return {"101", "101", "101", "101", "111"};
            case 'V': return {"101", "101", "101", "010", "010"};
            case 'W': return {"101", "101", "111", "111", "101"};
            case 'X': return {"101", "101", "010", "101", "101"};
            case 'Y': return {"101", "101", "010", "010", "010"};
            case 'Z': return {"111", "001", "010", "100", "111"};
            case '-': return {"000", "000", "111", "000", "000"};
            case ':': return {"000", "010", "000", "010", "000"};
            case ' ': return {"000", "000", "000", "000", "000"};
            case ']': return {"111", "001", "001", "001", "111"};
            case '[': return {"111", "100", "100", "100", "111"};
            case ')': return {"011", "001", "001", "001", "011"};
            case '(': return {"110", "100", "100", "100", "110"};
            case '|': return {"010", "010", "010", "010", "010"};
            case ',': return {"000", "000", "000", "000", "100"};
            case '.': return {"000", "000", "000", "000", "010"};
            default:  return {"111", "001", "011", "000", "010"};
        }
    }
}

DebugOverlay::~DebugOverlay() {
    if (vbo_ != 0 && pglDeleteBuffers) {
        pglDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0 && pglDeleteVertexArrays) {
        pglDeleteVertexArrays(1, &vao_);
    }
}

bool DebugOverlay::LoadOpenGLFunctions() {
    if (gOverlayGlLoaded) {
        return true;
    }

    pglGenVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(GLProc("glGenVertexArrays"));
    pglBindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(GLProc("glBindVertexArray"));
    pglDeleteVertexArrays = reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(GLProc("glDeleteVertexArrays"));
    pglGenBuffers = reinterpret_cast<PFNGLGENBUFFERSPROC>(GLProc("glGenBuffers"));
    pglBindBuffer = reinterpret_cast<PFNGLBINDBUFFERPROC>(GLProc("glBindBuffer"));
    pglDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(GLProc("glDeleteBuffers"));
    pglBufferData = reinterpret_cast<PFNGLBUFFERDATAPROC>(GLProc("glBufferData"));
    pglVertexAttribPointer = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(GLProc("glVertexAttribPointer"));
    pglEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(GLProc("glEnableVertexAttribArray"));

    gOverlayGlLoaded = pglGenVertexArrays && pglBindVertexArray && pglDeleteVertexArrays &&
        pglGenBuffers && pglBindBuffer && pglDeleteBuffers && pglBufferData &&
        pglVertexAttribPointer && pglEnableVertexAttribArray;
    return gOverlayGlLoaded;
}

bool DebugOverlay::Initialize(const std::string& vertexShaderPath, const std::string& fragmentShaderPath) {
    if (!LoadOpenGLFunctions()) {
        SDL_Log("DebugOverlay could not load OpenGL functions.");
        return false;
    }

    if (!shader_.LoadFromFiles(vertexShaderPath, fragmentShaderPath)) {
        return false;
    }

    pglGenVertexArrays(1, &vao_);
    pglGenBuffers(1, &vbo_);

    pglBindVertexArray(vao_);
    pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    pglBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, nullptr, GL_DYNAMIC_DRAW);
    pglVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
    pglEnableVertexAttribArray(0);
    pglBindBuffer(GL_ARRAY_BUFFER, 0);
    pglBindVertexArray(0);
    return true;
}

void DebugOverlay::AppendRect(std::vector<float>& vertices, float x, float y, float width, float height) {
    const float x1 = x + width;
    const float y1 = y + height;

    vertices.insert(vertices.end(), {
        x, y, x1, y, x1, y1,
        x, y, x1, y1, x, y1,
    });
}

void DebugOverlay::AppendGlyph(std::vector<float>& vertices, float x, float y, float scale, char glyph) {
    const std::array<const char*, 5> rows = GlyphRows(glyph);

    for (int row = 0; row < 5; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (rows[static_cast<size_t>(row)][col] == '1') {
                AppendRect(vertices, x + static_cast<float>(col) * scale, y + static_cast<float>(row) * scale, scale, scale);
            }
        }
    }
}

void DebugOverlay::DrawFps(int fps, int viewportWidth, int viewportHeight) {
    if (vao_ == 0 || vbo_ == 0) {
        return;
    }

    const std::string text = "FPS: " + std::to_string(fps);
    std::vector<float> vertices;
    vertices.reserve(text.size() * 5 * 3 * 12);

    const float scale = 4.0f;
    float cursorX = 16.0f;
    const float cursorY = 16.0f;
    for (char glyph : text) {
        AppendGlyph(vertices, cursorX, cursorY, scale, glyph);
        cursorX += scale * 4.0f;
    }

    const glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), 0.0f);

    const GLboolean wasDepthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    shader_.Use();
    shader_.SetMat4("uMVP", glm::value_ptr(projection));

    pglBindVertexArray(vao_);
    pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    pglBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 2));
    pglBindBuffer(GL_ARRAY_BUFFER, 0);
    pglBindVertexArray(0);

    if (wasDepthEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
    if (wasCullEnabled) {
        glEnable(GL_CULL_FACE);
    }
}

void DebugOverlay::DrawText(const std::string& text, float x, float y, float scale,
                            int viewportWidth, int viewportHeight) {
    if (vao_ == 0 || vbo_ == 0 || text.empty()) {
        return;
    }

    std::vector<float> vertices;
    vertices.reserve(text.size() * 5 * 3 * 12);

    float cursorX = x;
    for (char glyph : text) {
        AppendGlyph(vertices, cursorX, y, scale, glyph);
        cursorX += scale * 4.0f;
    }

    const glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(viewportWidth), static_cast<float>(viewportHeight), 0.0f);

    const GLboolean wasDepthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean wasCullEnabled = glIsEnabled(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

    shader_.Use();
    shader_.SetMat4("uMVP", glm::value_ptr(projection));

    pglBindVertexArray(vao_);
    pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    pglBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 2));
    pglBindBuffer(GL_ARRAY_BUFFER, 0);
    pglBindVertexArray(0);

    if (wasDepthEnabled) {
        glEnable(GL_DEPTH_TEST);
    }
    if (wasCullEnabled) {
        glEnable(GL_CULL_FACE);
    }
}