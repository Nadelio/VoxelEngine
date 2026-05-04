#pragma once

#include <string>

#include "DataFormat.hpp"

struct PhysicsConstants {
    float gravity               = 21.0f;
    float jumpSpeed             =  7.0f;
    float moveSpeed             =  4.5f;
    float crouchSlowdown        =  2.0f;
    float proneSlowdown         =  4.0f;
    float airResistance         =  8.0f;
    float acceleration          = 30.0f;
    float groundFriction        = 15.0f;
    float maxVelocity           =  6.0f;
    float fallingBlockFallSpeed =  5.0f;

    float standHeight       = 1.50f;
    float standEyeFromFeet  = 1.35f;
    float crouchHeight      = 1.25f;
    float crouchEyeFromFeet = 1.15f;
    float crawlHeight       = 0.80f;
    float crawlEyeFromFeet  = 0.70f;
};

// Loads key : value pairs from a DataFormat file into 'out'.
// Unknown keys are silently ignored. Returns false if the file cannot be opened.
inline bool LoadPhysicsConstants(const std::string& path, PhysicsConstants& out) {
    const auto doc = DataFormat::ParseFile(path);
    if (!doc) { return false; }

    auto readFloat = [&](const char* key, float& field) {
        if (const DataFormat::Value* v = doc->Get(key)) {
            if      (v->IsFloat()) { field = static_cast<float>(v->AsFloat()); }
            else if (v->IsInt())   { field = static_cast<float>(v->AsInt());   }
        }
    };

    readFloat("gravity",                  out.gravity);
    readFloat("jump_speed",               out.jumpSpeed);
    readFloat("move_speed",               out.moveSpeed);
    readFloat("crouch_slowdown",          out.crouchSlowdown);
    readFloat("prone_slowdown",           out.proneSlowdown);
    readFloat("air_resistance",           out.airResistance);
    readFloat("acceleration",             out.acceleration);
    readFloat("max_velocity",             out.maxVelocity);
    readFloat("ground_friction",          out.groundFriction);
    readFloat("falling_block_fall_speed", out.fallingBlockFallSpeed);
    readFloat("stand_height",             out.standHeight);
    readFloat("stand_eye_from_feet",      out.standEyeFromFeet);
    readFloat("crouch_height",            out.crouchHeight);
    readFloat("crouch_eye_from_feet",     out.crouchEyeFromFeet);
    readFloat("crawl_height",             out.crawlHeight);
    readFloat("crawl_eye_from_feet",      out.crawlEyeFromFeet);
    return true;
}
