// Tests headless de ProjectSettings (F3H1): roundtrip, back-compat,
// forward-compat, malformed input.

#include <doctest/doctest.h>

#include "engine/project/ProjectSettings.h"

#include <nlohmann/json.hpp>

using namespace Mood;

TEST_CASE("ProjectSettings defaults: toJson devuelve object vacio") {
    ProjectSettings s;
    const auto j = toJson(s);
    CHECK(j.is_object());
    CHECK(j.empty());  // ningun field difiere del default
}

TEST_CASE("ProjectSettings non-default: toJson solo escribe lo que cambio") {
    ProjectSettings s;
    s.targetFps = 120;
    const auto j = toJson(s);
    CHECK(j.contains("target_fps"));
    CHECK(j.at("target_fps") == 120);
}

TEST_CASE("ProjectSettings roundtrip preserva el campo") {
    ProjectSettings before;
    before.targetFps = 144;

    const auto j = toJson(before);
    const auto after = projectSettingsFromJson(j);

    CHECK(after.targetFps == 144);
}

TEST_CASE("ProjectSettings fromJson de empty object devuelve defaults") {
    const auto s = projectSettingsFromJson(nlohmann::json::object());
    CHECK(s.targetFps == 60);
}

TEST_CASE("ProjectSettings fromJson de non-object devuelve defaults") {
    // null, string, array → no son object de settings
    const auto s1 = projectSettingsFromJson(nlohmann::json{});
    const auto s2 = projectSettingsFromJson(nlohmann::json("hello"));
    const auto s3 = projectSettingsFromJson(nlohmann::json::array({1, 2, 3}));
    CHECK(s1.targetFps == 60);
    CHECK(s2.targetFps == 60);
    CHECK(s3.targetFps == 60);
}

TEST_CASE("ProjectSettings fromJson forward-compat: keys desconocidas ignoradas") {
    // Simula un .moodproj escrito por una version futura del editor
    // que agrego nuevos campos.
    nlohmann::json j;
    j["target_fps"]      = 90;
    j["future_field_x"]  = 42;            // futuro: codigo viejo no la conoce
    j["future_field_y"]  = "anything";    // futuro: idem
    j["nested_future"]   = {{"a", 1}};    // futuro: subobject anidado

    // Debe cargar sin throw ni warning del field conocido.
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps == 90);
}

TEST_CASE("ProjectSettings fromJson tipo incorrecto: skip silencioso del field") {
    // target_fps esperado int pero llega string → field skipped, default.
    nlohmann::json j;
    j["target_fps"] = "no-soy-int";
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps == 60);            // default
}
