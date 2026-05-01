#pragma once

#include <SDL3/SDL.h>

#include <glm/glm.hpp>

class Camera {
public:
    glm::vec3 position{0.0f};
    float yawRadians = -1.57079f;
    float pitchRadians = -0.3f;

    glm::vec3 Forward() const;
    glm::mat4 View() const;

    void UpdateFromKeyboard(const bool* keys, float deltaSeconds);
    void UpdateFromMouseDelta(float deltaX, float deltaY);
};
