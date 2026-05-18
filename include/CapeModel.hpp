#pragma once

#include <string>
#include <cstdint>

#include <SDL3/SDL_opengl.h>

#include <glm/glm.hpp>

#include "Shader.hpp"
#include "SkinTexture.hpp"

// Simple cape model that renders as a flat quadrilateral attached to the player's back
class CapeModel {
public:
    CapeModel() = default;
    ~CapeModel();

    CapeModel(const CapeModel&) = delete;
    CapeModel& operator=(const CapeModel&) = delete;

    bool Initialize();
    bool LoadCape(const std::string& capePath);

    void Draw(
        Shader& capeShader,
        const glm::mat4& projection,
        const glm::mat4& view,
        const glm::mat4& modelMatrix,
        float capeFlap,
        float capeLean,
        float capeLean2
    ) const;

    void DrawShadow(
        Shader& shadowShader,
        const glm::mat4& lightSpaceMatrix,
        const glm::mat4& modelMatrix,
        float capeFlap,
        float capeLean,
        float capeLean2
    ) const;

    bool IsLoaded() const { return capeTexture_.TextureID() != 0; }

private:
    bool CreateCapeMesh();

    struct PrimitiveGpu {
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint ebo = 0;
        GLsizei indexCount = 0;
    };

    PrimitiveGpu primitive_;
    SkinTexture capeTexture_;
    bool initialized_ = false;
};
