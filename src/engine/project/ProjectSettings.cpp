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

    return s;
}

} // namespace Mood
