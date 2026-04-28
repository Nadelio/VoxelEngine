#include "Camera.hpp"

#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

glm::vec3 Camera::Forward() const {
    const glm::vec3 f(
        std::cos(yawRadians) * std::cos(pitchRadians),
        std::sin(pitchRadians),
        std::sin(yawRadians) * std::cos(pitchRadians)
    );
    return glm::normalize(f);
}

glm::mat4 Camera::View() const {
    return glm::lookAt(position, position + Forward(), glm::vec3(0.0f, 1.0f, 0.0f));
}

void Camera::UpdateFromKeyboard(const bool* keys, float deltaSeconds) {
    const float moveSpeed = 2.5f * deltaSeconds;

    glm::vec3 forward = Forward();
    forward.y = 0.0f;
    forward = glm::normalize(forward);

    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (keys[SDL_SCANCODE_W]) position += forward * moveSpeed;
    if (keys[SDL_SCANCODE_S]) position -= forward * moveSpeed;
    if (keys[SDL_SCANCODE_A]) position -= right * moveSpeed;
    if (keys[SDL_SCANCODE_D]) position += right * moveSpeed;
    if (keys[SDL_SCANCODE_SPACE]) position.y += moveSpeed;
    if (keys[SDL_SCANCODE_LCTRL]) position.y -= moveSpeed;
}

void Camera::UpdateFromMouseDelta(float deltaX, float deltaY) {
    const float sensitivity = 0.0022f;

    yawRadians += deltaX * sensitivity;
    pitchRadians -= deltaY * sensitivity;

    if (pitchRadians > 1.5f) pitchRadians = 1.5f;
    if (pitchRadians < -1.5f) pitchRadians = -1.5f;
}
