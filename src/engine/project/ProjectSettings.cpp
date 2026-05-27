#include "engine/project/ProjectSettings.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <set>

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

// F3H6: helpers para SnapSettings con validacion estricta del array de
// steps (positivos, unicos, ordenados ascendente).
nlohmann::json snapToJson(const SnapSettings& s) {
    nlohmann::json j = nlohmann::json::object();
    const SnapSettings defaults;
    if (s.stepsAvailable != defaults.stepsAvailable) j["steps_available"] = s.stepsAvailable;
    if (s.defaultStepIndex != defaults.defaultStepIndex) j["default_step_index"] = s.defaultStepIndex;
    if (s.snapToVertexThresholdNdc != defaults.snapToVertexThresholdNdc) j["vertex_threshold_ndc"] = s.snapToVertexThresholdNdc;
    if (s.snapBroadphaseMinWorld != defaults.snapBroadphaseMinWorld) j["broadphase_min_world"] = s.snapBroadphaseMinWorld;
    // F3H20: toggles + increments.
    if (s.snapToVertexEnabled  != defaults.snapToVertexEnabled)  j["vertex_enabled"]  = s.snapToVertexEnabled;
    if (s.snapGridEnabled      != defaults.snapGridEnabled)      j["grid_enabled"]    = s.snapGridEnabled;
    if (s.snapGridStep         != defaults.snapGridStep)         j["grid_step"]       = s.snapGridStep;
    if (s.snapAngleEnabled     != defaults.snapAngleEnabled)     j["angle_enabled"]   = s.snapAngleEnabled;
    if (s.snapAngleDegrees     != defaults.snapAngleDegrees)     j["angle_degrees"]   = s.snapAngleDegrees;
    return j;
}

SnapSettings snapFromJson(const nlohmann::json& j) {
    SnapSettings s;
    if (!j.is_object()) return s;

    // Array de steps: solo aceptamos si es array de ints positivos.
    // Sanitize: filtrar non-int + < 1, sort ascendente + dedupe. Si el
    // resultado es vacio, usar defaults (no dejar al dev sin steps).
    if (j.contains("steps_available") && j.at("steps_available").is_array()) {
        std::set<int> uniqueSteps;
        for (const auto& elem : j.at("steps_available")) {
            if (elem.is_number_integer()) {
                const int v = elem.get<int>();
                if (v > 0) uniqueSteps.insert(v);
            }
        }
        if (!uniqueSteps.empty()) {
            s.stepsAvailable.assign(uniqueSteps.begin(), uniqueSteps.end());
        }
        // else: array sanitizado quedo vacio → mantener defaults.
    }

    if (j.contains("default_step_index") && j.at("default_step_index").is_number_integer()) {
        s.defaultStepIndex = j.at("default_step_index").get<int>();
    }
    // Clamp del index al rango valido del array final.
    if (s.defaultStepIndex < 0
        || s.defaultStepIndex >= static_cast<int>(s.stepsAvailable.size())) {
        s.defaultStepIndex = 0;
    }

    if (j.contains("vertex_threshold_ndc") && j.at("vertex_threshold_ndc").is_number()) {
        s.snapToVertexThresholdNdc = j.at("vertex_threshold_ndc").get<f32>();
    }
    if (j.contains("broadphase_min_world") && j.at("broadphase_min_world").is_number()) {
        s.snapBroadphaseMinWorld = j.at("broadphase_min_world").get<f32>();
    }

    // F3H20: toggles + increments (sanitize sin log — defaults razonables
    // si el dev edito a mano y quedo invalido).
    if (j.contains("vertex_enabled") && j.at("vertex_enabled").is_boolean()) {
        s.snapToVertexEnabled = j.at("vertex_enabled").get<bool>();
    }
    if (j.contains("grid_enabled") && j.at("grid_enabled").is_boolean()) {
        s.snapGridEnabled = j.at("grid_enabled").get<bool>();
    }
    if (j.contains("grid_step") && j.at("grid_step").is_number()) {
        s.snapGridStep = j.at("grid_step").get<f32>();
        if (s.snapGridStep <= 0.0f) {
            s.snapGridStep = SnapSettings{}.snapGridStep;
        }
    }
    if (j.contains("angle_enabled") && j.at("angle_enabled").is_boolean()) {
        s.snapAngleEnabled = j.at("angle_enabled").get<bool>();
    }
    if (j.contains("angle_degrees") && j.at("angle_degrees").is_number()) {
        s.snapAngleDegrees = j.at("angle_degrees").get<f32>();
        if (s.snapAngleDegrees <= 0.0f || s.snapAngleDegrees > 360.0f) {
            s.snapAngleDegrees = SnapSettings{}.snapAngleDegrees;
        }
    }

    return s;
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

    // F3H6: subobjeto "snap" solo si difiere del default.
    nlohmann::json snapJson = snapToJson(s.snap);
    if (!snapJson.empty()) {
        j["snap"] = std::move(snapJson);
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

    // F3H6: subobjeto "snap" — mismo patron.
    if (j.contains("snap")) {
        s.snap = snapFromJson(j.at("snap"));
    }

    return s;
}

} // namespace Mood
