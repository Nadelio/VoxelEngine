#include "Physics.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "Camera.hpp"

namespace {
bool BlockIntersectsAabb(const glm::ivec3& blockPos, const Physics::AABB& aabb) {
    const glm::vec3 blockMin = glm::vec3(blockPos) - glm::vec3(0.5f);
    const glm::vec3 blockMax = glm::vec3(blockPos) + glm::vec3(0.5f);
    constexpr float kEps = 0.0001f;
    return (aabb.min.x < blockMax.x - kEps && aabb.max.x > blockMin.x + kEps) &&
           (aabb.min.y < blockMax.y - kEps && aabb.max.y > blockMin.y + kEps) &&
           (aabb.min.z < blockMax.z - kEps && aabb.max.z > blockMin.z + kEps);
}
}

Physics::AABB Physics::EntityAABB(const Entity& entity) const {
    return {
        glm::vec3(entity.position.x - entity.radius,
                  entity.position.y - entity.eyeFromFeet,
                  entity.position.z - entity.radius),
        glm::vec3(entity.position.x + entity.radius,
                  entity.position.y + (entity.height - entity.eyeFromFeet),
                  entity.position.z + entity.radius)
    };
}

bool Physics::IntersectsWorld(const AABB& aabb) const {
    const int minX = static_cast<int>(std::ceil(aabb.min.x - 0.5f + 0.0001f));
    const int minY = static_cast<int>(std::ceil(aabb.min.y - 0.5f + 0.0001f));
    const int minZ = static_cast<int>(std::ceil(aabb.min.z - 0.5f + 0.0001f));
    const int maxX = static_cast<int>(std::floor(aabb.max.x + 0.5f - 0.0001f));
    const int maxY = static_cast<int>(std::floor(aabb.max.y + 0.5f - 0.0001f));
    const int maxZ = static_cast<int>(std::floor(aabb.max.z + 0.5f - 0.0001f));

    for (int x = minX; x <= maxX; ++x) {
        for (int y = minY; y <= maxY; ++y) {
            for (int z = minZ; z <= maxZ; ++z) {
                if (grid_.HasBlockAt({x, y, z})) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool Physics::MoveEntityAxis(Entity& entity, int axis, float delta) const {
    if (std::fabs(delta) < 0.00001f) {
        return false;
    }

    bool collided = false;
    const int steps = std::max(1, static_cast<int>(std::ceil(std::fabs(delta) / kAxisStep)));
    const float stepDelta = delta / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        Entity candidate = entity;
        candidate.position[axis] += stepDelta;
        if (IntersectsWorld(EntityAABB(candidate))) {
            collided = true;
            break;
        }
        entity.position = candidate.position;
    }

    return collided;
}

bool Physics::CanFitPosture(const Entity& entity, float newHeight, float newEyeFromFeet) const {
    Entity test = entity;
    const float feetY = entity.position.y - entity.eyeFromFeet;
    test.height = newHeight;
    test.eyeFromFeet = newEyeFromFeet;
    test.position.y = feetY + newEyeFromFeet;
    return !IntersectsWorld(EntityAABB(test));
}

void Physics::StepEntityEuler(Entity& entity,
                              float deltaSeconds,
                              const glm::vec3& desiredHorizontalVelocity,
                              bool jumpRequested,
                              bool crouchHeld,
                              bool crawlToggleThisFrame,
                              const PhysicsConstants& constants) {
    if (crawlToggleThisFrame) {
        entity.crawlActive = !entity.crawlActive;
    }

    PostureState desiredPosture = PostureState::STANDING;
    if (entity.crawlActive) {
        desiredPosture = PostureState::CRAWLING;
    } else if (crouchHeld) {
        desiredPosture = PostureState::CROUCHING;
    }

    auto ApplyPosture = [&](PostureState posture, float newHeight, float newEyeFromFeet) {
        const float feetY = entity.position.y - entity.eyeFromFeet;
        entity.posture = posture;
        entity.height = newHeight;
        entity.eyeFromFeet = newEyeFromFeet;
        entity.position.y = feetY + newEyeFromFeet;
    };

    if (desiredPosture != entity.posture) {
        float newH   = constants.standHeight;
        float newEye = constants.standEyeFromFeet;
        if (desiredPosture == PostureState::CROUCHING) {
            newH   = constants.crouchHeight;
            newEye = constants.crouchEyeFromFeet;
        } else if (desiredPosture == PostureState::CRAWLING) {
            newH   = constants.crawlHeight;
            newEye = constants.crawlEyeFromFeet;
        }

        const bool goingTaller = (newH > entity.height);
        if (!goingTaller || CanFitPosture(entity, newH, newEye)) {
            ApplyPosture(desiredPosture, newH, newEye);
        } else if (desiredPosture == PostureState::STANDING &&
                   entity.posture == PostureState::CRAWLING) {
            if (CanFitPosture(entity, constants.crouchHeight, constants.crouchEyeFromFeet)) {
                ApplyPosture(PostureState::CROUCHING,
                             constants.crouchHeight,
                             constants.crouchEyeFromFeet);
            }
        }
    }

    float slowdown = 0.0f;
    if (entity.posture == PostureState::CROUCHING) {
        slowdown = std::max(0.0f, constants.crouchSlowdown);
    } else if (entity.posture == PostureState::CRAWLING) {
        slowdown = std::max(0.0f, constants.proneSlowdown);
    }

    glm::vec2 desired2D(desiredHorizontalVelocity.x, desiredHorizontalVelocity.z);
    const float desiredSpeed = glm::length(desired2D);
    if (desiredSpeed > 0.0001f) {
        const float adjustedSpeed = std::max(0.0f, desiredSpeed - slowdown);
        desired2D = glm::normalize(desired2D) * adjustedSpeed;
    }
    glm::vec2 current2D(entity.velocity.x, entity.velocity.z);

    if (glm::length2(desired2D) > 0.0000001f) {
        const glm::vec2 diff    = desired2D - current2D;
        const float     diffLen = glm::length(diff);
        const float     step    = constants.acceleration * deltaSeconds;
        current2D += (step >= diffLen) ? diff : (glm::normalize(diff) * step);
    } else {
        const float speed = glm::length(current2D);
        const float dragRate = entity.onGround ? constants.groundFriction : constants.airResistance;
        const float drag  = dragRate * deltaSeconds;
        if (speed <= drag) {
            current2D = glm::vec2(0.0f);
        } else {
            current2D -= glm::normalize(current2D) * drag;
        }
    }

    const float hSpeed = glm::length(current2D);
    if (hSpeed > constants.maxVelocity) {
        current2D *= (constants.maxVelocity / hSpeed);
    }

    entity.velocity.x = current2D.x;
    entity.velocity.z = current2D.y;

    if (entity.onGround && jumpRequested) {
        entity.velocity.y = constants.jumpSpeed;
        entity.onGround   = false;
    } else if (entity.onGround) {
        entity.velocity.y = -constants.gravity * deltaSeconds;
    } else {
        entity.velocity.y -= constants.gravity * deltaSeconds;
    }

    MoveEntityAxis(entity, 0, entity.velocity.x * deltaSeconds);
    MoveEntityAxis(entity, 2, entity.velocity.z * deltaSeconds);

    const bool collidedY = MoveEntityAxis(entity, 1, entity.velocity.y * deltaSeconds);
    if (collidedY) {
        if (entity.velocity.y < 0.0f) {
            entity.onGround = true;
        }
        entity.velocity.y = 0.0f;
    } else {
        Entity probe     = entity;
        probe.position.y -= kGroundProbe;
        entity.onGround  = IntersectsWorld(EntityAABB(probe));
    }
}

void Physics::StepBlockGravity(float deltaSeconds) {
    blockGravityAccumulator_ += deltaSeconds;

    while (blockGravityAccumulator_ >= kBlockStepSeconds) {
        blockGravityAccumulator_ -= kBlockStepSeconds;

        std::vector<std::pair<glm::ivec3, uint32_t>> gravityBlocks;
        grid_.VisitBlocks([&](const glm::ivec3& pos, uint32_t blockID) {
            const BlockData* data = registry_.Get(blockID);
            if (data && data->affectedByGravity) {
                gravityBlocks.emplace_back(pos, blockID);
            }
        });

        std::sort(gravityBlocks.begin(), gravityBlocks.end(),
                  [](const std::pair<glm::ivec3, uint32_t>& a,
                     const std::pair<glm::ivec3, uint32_t>& b) {
                      return a.first.y < b.first.y;
                  });

        for (const auto& block : gravityBlocks) {
            const glm::ivec3& pos = block.first;
            const uint32_t blockID = block.second;

            if (!grid_.HasBlockAt(pos)) {
                continue;
            }

            const glm::ivec3 below = pos + glm::ivec3(0, -1, 0);
            if (grid_.HasBlockAt(below)) {
                continue;
            }
            
            if (grid_.RemoveBlock(pos)) {
                fallingBlocks_.push_back({below, glm::vec3(pos), blockID});
            }
        }
    }
}

void Physics::UpdateFallingBlocks(float deltaSeconds) {
    const float kFallSpeed = constants_.fallingBlockFallSpeed;

    for (int i = static_cast<int>(fallingBlocks_.size()) - 1; i >= 0; --i) {
        FallingBlock& fb = fallingBlocks_[i];
        fb.pos.y -= kFallSpeed * deltaSeconds;

        if (fb.pos.y < -200.0f) {
            fallingBlocks_.erase(fallingBlocks_.begin() + i);
            continue;
        }

        const float targetY = static_cast<float>(fb.gridTarget.y);
        if (fb.pos.y > targetY) {
            continue;
        }

        const glm::ivec3 below = fb.gridTarget + glm::ivec3(0, -1, 0);
        if (!grid_.HasBlockAt(below) && !grid_.HasBlockAt(fb.gridTarget)) {
            fb.gridTarget = below;
            continue;
        }

        glm::ivec3 landPos = fb.gridTarget;
        for (int tries = 0; tries < 4 && grid_.HasBlockAt(landPos); ++tries) {
            landPos.y += 1;
        }
        if (!grid_.HasBlockAt(landPos)) {
            grid_.AddBlock(landPos.x, landPos.y, landPos.z, fb.blockID);
        }
        fallingBlocks_.erase(fallingBlocks_.begin() + i);
    }
}

bool Physics::CanPlaceBlockAt(const Entity& entity,
                              const Camera& camera,
                              const glm::ivec3& placePos) const {
    const AABB entityAabb = EntityAABB(entity);

    if (BlockIntersectsAabb(placePos, entityAabb)) {
        return false;
    }

    if (IntersectsWorld(entityAabb)) {
        glm::vec3 toPlace = glm::vec3(placePos) - entity.position;
        if (glm::length2(toPlace) > 0.0000001f) {
            toPlace = glm::normalize(toPlace);
            const glm::vec3 forward = glm::normalize(camera.Forward());
            if (glm::dot(forward, toPlace) < 0.0f) {
                return false;
            }
        }
    }

    return true;
}

bool Physics::ForceEntityUpIfInsideBlock(Entity& entity) const {
    if (!IntersectsWorld(EntityAABB(entity))) {
        return false;
    }

    constexpr float kPushStep = 0.05f;
    constexpr int kMaxPushSteps = 200;
    for (int i = 0; i < kMaxPushSteps && IntersectsWorld(EntityAABB(entity)); ++i) {
        entity.position.y += kPushStep;
    }

    if (entity.velocity.y < 0.0f) {
        entity.velocity.y = 0.0f;
    }
    entity.onGround = false;
    return true;
}

void Physics::teleportTo(Entity& entity, const glm::vec3& newPosition, Camera* camera) const {
    entity.teleportTo(newPosition);
    if (camera != nullptr) {
        camera->position = entity.position;
    }
}
