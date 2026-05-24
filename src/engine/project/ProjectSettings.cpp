#include "engine/project/ProjectSettings.h"

#include <nlohmann/json.hpp>

namespace Mood {

namespace {

// F3H4: helper para serializar GameplaySettings. Mismo patron defensivo
// que ProjectSettings — solo escribe fields que difieren del default.
nlohmann::json gameplayToJson(const GameplaySettings& g) {
    nlohmann::json j = nlohmann::json::object();
    const GameplaySettings defaults;
    if (g.walkSpeed       != defaults.walkSpeed)       j["walk_speed"]        = g.walkSpeed;
    if (g.crouchSpeed     != defaults.crouchSpeed)     j["crouch_speed"]      = g.crouchSpeed;
    if (g.jumpVelocity    != defaults.jumpVelocity)    j["jump_velocity"]     = g.jumpVelocity;
    if (g.jumpCooldownSec != defaults.jumpCooldownSec) j["jump_cooldown_sec"] = g.jumpCooldownSec;
    return j;
}

GameplaySettings gameplayFromJson(const nlohmann::json& j) {
    GameplaySettings g;
    if (!j.is_object()) return g;
    if (j.contains("walk_speed")        && j.at("walk_speed").is_number())        g.walkSpeed        = j.at("walk_speed").get<f32>();
    if (j.contains("crouch_speed")      && j.at("crouch_speed").is_number())      g.crouchSpeed      = j.at("crouch_speed").get<f32>();
    if (j.contains("jump_velocity")     && j.at("jump_velocity").is_number())     g.jumpVelocity     = j.at("jump_velocity").get<f32>();
    if (j.contains("jump_cooldown_sec") && j.at("jump_cooldown_sec").is_number()) g.jumpCooldownSec  = j.at("jump_cooldown_sec").get<f32>();
    return g;
}

// F3H5: helpers para CharacterSettings. Mismo patron defensivo que
// gameplay — solo escribe fields que difieren del default, lee solo
// los que estan presentes con type-check.
nlohmann::json characterToJson(const CharacterSettings& c) {
    nlohmann::json j = nlohmann::json::object();
    const CharacterSettings defaults;
    if (c.halfHeightStand   != defaults.halfHeightStand)   j["half_height_stand"]  = c.halfHeightStand;
    if (c.halfHeightCrouch  != defaults.halfHeightCrouch)  j["half_height_crouch"] = c.halfHeightCrouch;
    if (c.radius            != defaults.radius)            j["radius"]             = c.radius;
    if (c.eyeHeightStand    != defaults.eyeHeightStand)    j["eye_height_stand"]   = c.eyeHeightStand;
    if (c.eyeHeightCrouch   != defaults.eyeHeightCrouch)   j["eye_height_crouch"]  = c.eyeHeightCrouch;
    if (c.headbobFrequency  != defaults.headbobFrequency)  j["headbob_frequency"]  = c.headbobFrequency;
    if (c.headbobAmplitude  != defaults.headbobAmplitude)  j["headbob_amplitude"]  = c.headbobAmplitude;
    return j;
}

CharacterSettings characterFromJson(const nlohmann::json& j) {
    CharacterSettings c;
    if (!j.is_object()) return c;
    if (j.contains("half_height_stand")  && j.at("half_height_stand").is_number())  c.halfHeightStand   = j.at("half_height_stand").get<f32>();
    if (j.contains("half_height_crouch") && j.at("half_height_crouch").is_number()) c.halfHeightCrouch  = j.at("half_height_crouch").get<f32>();
    if (j.contains("radius")             && j.at("radius").is_number())             c.radius            = j.at("radius").get<f32>();
    if (j.contains("eye_height_stand")   && j.at("eye_height_stand").is_number())   c.eyeHeightStand    = j.at("eye_height_stand").get<f32>();
    if (j.contains("eye_height_crouch")  && j.at("eye_height_crouch").is_number())  c.eyeHeightCrouch   = j.at("eye_height_crouch").get<f32>();
    if (j.contains("headbob_frequency")  && j.at("headbob_frequency").is_number())  c.headbobFrequency  = j.at("headbob_frequency").get<f32>();
    if (j.contains("headbob_amplitude")  && j.at("headbob_amplitude").is_number())  c.headbobAmplitude  = j.at("headbob_amplitude").get<f32>();
    return c;
}

} // namespace

nlohmann::json toJson(const ProjectSettings& s) {
    nlohmann::json j = nlohmann::json::object();

    const ProjectSettings defaults;

    if (s.targetFps != defaults.targetFps) {
        j["target_fps"] = s.targetFps;
    }

    // F3H4: subobjeto "gameplay" solo si difiere del default.
    nlohmann::json gameplayJson = gameplayToJson(s.gameplay);
    if (!gameplayJson.empty()) {
        j["gameplay"] = std::move(gameplayJson);
    }

    // F3H5: subobjeto "character" solo si difiere del default.
    nlohmann::json characterJson = characterToJson(s.character);
    if (!characterJson.empty()) {
        j["character"] = std::move(characterJson);
    }

    return j;
}

ProjectSettings projectSettingsFromJson(const nlohmann::json& j) {
    ProjectSettings s;
    if (!j.is_object()) return s;

    if (j.contains("target_fps") && j.at("target_fps").is_number_integer()) {
        s.targetFps = j.at("target_fps").get<int>();
    }

    // F3H4: leer subobjeto "gameplay" si presente. Si falta o no es
    // object, gameplayFromJson devuelve defaults — back-compat con
    // .moodproj pre-F3H4.
    if (j.contains("gameplay")) {
        s.gameplay = gameplayFromJson(j.at("gameplay"));
    }

    // F3H5: subobjeto "character" — mismo patron.
    if (j.contains("character")) {
        s.character = characterFromJson(j.at("character"));
    }

    return s;
}

} // namespace Mood
