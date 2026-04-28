#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "BlockRegistry.hpp"
#include "Grid.hpp"

class Camera;

class Physics {
public:
    struct AABB {
        glm::vec3 min{};
        glm::vec3 max{};
    };

    struct Entity {
        glm::vec3 position{0.0f};
        glm::vec3 velocity{0.0f};

        float radius = 0.30f;
        float height = 1.80f;
        float eyeFromFeet = 1.62f;

        bool onGround = false;

        void teleportTo(const glm::vec3& newPosition) {
            position = newPosition;
            velocity = glm::vec3(0.0f);
            onGround = false;
        }
    };

    // A gravity block that was removed from the integer grid and is falling smoothly.
    struct FallingBlock {
        glm::ivec3 gridTarget;  // integer grid cell currently being targeted
        glm::vec3  pos;         // continuous visual position (center of block)
        uint32_t   blockID;
    };

    Physics(Grid& grid, const BlockRegistry& registry) : grid_(grid), registry_(registry) {}

    void StepEntityEuler(Entity& entity,
                         float deltaSeconds,
                         const glm::vec3& desiredHorizontalVelocity,
                         bool jumpRequested,
                         float gravityAcceleration,
                         float jumpSpeed);

    void StepBlockGravity(float deltaSeconds);

    // Smoothly advance all in-flight falling blocks and land them when they arrive.
    void UpdateFallingBlocks(float deltaSeconds);

    const std::vector<FallingBlock>& GetFallingBlocks() const { return fallingBlocks_; }

    // Returns true when a block may be placed at placePos for this entity/camera state.
    bool CanPlaceBlockAt(const Entity& entity, const Camera& camera, const glm::ivec3& placePos) const;

    // If entity is intersecting blocks, pushes it upward until clear (or safety cap).
    // Returns true if a push was applied.
    bool ForceEntityUpIfInsideBlock(Entity& entity) const;

    void teleportTo(Entity& entity, const glm::vec3& newPosition, Camera* camera = nullptr) const;

private:
    AABB EntityAABB(const Entity& entity) const;
    bool IntersectsWorld(const AABB& aabb) const;
    bool MoveEntityAxis(Entity& entity, int axis, float delta) const;

    Grid& grid_;
    const BlockRegistry& registry_;

    float blockGravityAccumulator_ = 0.0f;
    std::vector<FallingBlock> fallingBlocks_;

    static constexpr float kAxisStep = 0.05f;
    static constexpr float kGroundProbe = 0.03f;
    static constexpr float kBlockStepSeconds = 0.12f;
};
