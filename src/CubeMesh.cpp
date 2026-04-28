#include "CubeMesh.hpp"

#include <cstddef>
#include <cstdint>

#include <SDL3/SDL.h>

#include "MeshConstants.hpp"

namespace {
    PFNGLGENVERTEXARRAYSPROC pglGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC pglBindVertexArray = nullptr;
    PFNGLGENBUFFERSPROC pglGenBuffers = nullptr;
    PFNGLBINDBUFFERPROC pglBindBuffer = nullptr;
    PFNGLBUFFERDATAPROC pglBufferData = nullptr;
    PFNGLBUFFERSUBDATAPROC pglBufferSubData = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC pglVertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC pglEnableVertexAttribArray = nullptr;
    PFNGLDELETEVERTEXARRAYSPROC pglDeleteVertexArrays = nullptr;
    PFNGLDELETEBUFFERSPROC pglDeleteBuffers = nullptr;
}

CubeMesh::~CubeMesh() {
    if (ebo_ != 0 && pglDeleteBuffers) {
        pglDeleteBuffers(1, &ebo_);
    }
    if (vbo_ != 0 && pglDeleteBuffers) {
        pglDeleteBuffers(1, &vbo_);
    }
    if (vao_ != 0 && pglDeleteVertexArrays) {
        pglDeleteVertexArrays(1, &vao_);
    }
}

bool CubeMesh::LoadMeshGLFunctions() {
    pglGenVertexArrays = reinterpret_cast<PFNGLGENVERTEXARRAYSPROC>(SDL_GL_GetProcAddress("glGenVertexArrays"));
    pglBindVertexArray = reinterpret_cast<PFNGLBINDVERTEXARRAYPROC>(SDL_GL_GetProcAddress("glBindVertexArray"));
    pglGenBuffers = reinterpret_cast<PFNGLGENBUFFERSPROC>(SDL_GL_GetProcAddress("glGenBuffers"));
    pglBindBuffer = reinterpret_cast<PFNGLBINDBUFFERPROC>(SDL_GL_GetProcAddress("glBindBuffer"));
    pglBufferData = reinterpret_cast<PFNGLBUFFERDATAPROC>(SDL_GL_GetProcAddress("glBufferData"));
    pglBufferSubData = reinterpret_cast<PFNGLBUFFERSUBDATAPROC>(SDL_GL_GetProcAddress("glBufferSubData"));
    pglVertexAttribPointer = reinterpret_cast<PFNGLVERTEXATTRIBPOINTERPROC>(SDL_GL_GetProcAddress("glVertexAttribPointer"));
    pglEnableVertexAttribArray = reinterpret_cast<PFNGLENABLEVERTEXATTRIBARRAYPROC>(SDL_GL_GetProcAddress("glEnableVertexAttribArray"));
    pglDeleteVertexArrays = reinterpret_cast<PFNGLDELETEVERTEXARRAYSPROC>(SDL_GL_GetProcAddress("glDeleteVertexArrays"));
    pglDeleteBuffers = reinterpret_cast<PFNGLDELETEBUFFERSPROC>(SDL_GL_GetProcAddress("glDeleteBuffers"));

    return pglGenVertexArrays && pglBindVertexArray && pglGenBuffers && pglBindBuffer &&
           pglBufferData && pglBufferSubData && pglVertexAttribPointer && pglEnableVertexAttribArray &&
           pglDeleteVertexArrays && pglDeleteBuffers;
}

bool CubeMesh::Initialize() {
    if (!LoadMeshGLFunctions()) {
        SDL_Log("CubeMesh: failed to load required OpenGL functions.");
        return false;
    }

    vertices_ = MeshConstants::cube_mesh;

    pglGenVertexArrays(1, &vao_);
    pglGenBuffers(1, &vbo_);
    pglGenBuffers(1, &ebo_);

    pglBindVertexArray(vao_);

    pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    pglBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices_.size() * sizeof(float)),
        vertices_.data(),
        GL_DYNAMIC_DRAW
    );

    pglBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    pglBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(MeshConstants::kCubeIndices.size() * sizeof(std::uint32_t)),
        MeshConstants::kCubeIndices.data(),
        GL_STATIC_DRAW
    );

    pglVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * static_cast<GLsizei>(sizeof(float)), reinterpret_cast<void*>(0));
    pglEnableVertexAttribArray(0);
    pglVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        5 * static_cast<GLsizei>(sizeof(float)),
        reinterpret_cast<void*>(3 * sizeof(float))
    );
    pglEnableVertexAttribArray(1);

    pglBindVertexArray(0);
    return true;
}

void CubeMesh::SetFaceTiles(const AtlasTexture& atlas, const FaceTileMap& faceTiles) {
    for (int face = 0; face < 6; ++face) {
        const std::array<float, 8> tileUV = atlas.TileUV32(faceTiles[face].x, faceTiles[face].y);
        for (int localVertex = 0; localVertex < 4; ++localVertex) {
            const int vertexIndex = face * 4 + localVertex;
            const int base = vertexIndex * 5;
            vertices_[base + 3] = tileUV[localVertex * 2 + 0];
            vertices_[base + 4] = tileUV[localVertex * 2 + 1];
        }
    }

    pglBindBuffer(GL_ARRAY_BUFFER, vbo_);
    pglBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(vertices_.size() * sizeof(float)),
        vertices_.data()
    );
}

void CubeMesh::SetVisibleFaces(uint8_t faceMask) {
    visibleFaces_ = faceMask;
}

void CubeMesh::Draw() const {
    pglBindVertexArray(vao_);
    for (int face = 0; face < 6; ++face) {
        if (visibleFaces_ & (1u << face)) {
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                           reinterpret_cast<void*>(face * 6 * sizeof(std::uint32_t)));
        }
    }
}
