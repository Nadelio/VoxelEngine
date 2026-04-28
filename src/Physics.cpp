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

void Physics::StepEntityEuler(Entity& entity,
                              float deltaSeconds,
                              const glm::vec3& desiredHorizontalVelocity,
                              bool jumpRequested,
                              float gravityAcceleration,
                              float jumpSpeed) {
    entity.velocity.x = desiredHorizontalVelocity.x;
    entity.velocity.z = desiredHorizontalVelocity.z;

    if (entity.onGround && jumpRequested) {
        entity.velocity.y = jumpSpeed;
        entity.onGround = false;
    } else if (entity.onGround) {
        entity.velocity.y = -gravityAcceleration * deltaSeconds;
    } else {
        entity.velocity.y -= gravityAcceleration * deltaSeconds;
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
        Entity probe = entity;
        probe.position.y -= kGroundProbe;
        entity.onGround = IntersectsWorld(EntityAABB(probe));
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
    constexpr float kFallSpeed = 5.0f;

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
        if (glm::length(toPlace) > 0.0001f) {
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
