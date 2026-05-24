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

TEST_CASE("ProjectSettings non-defaults: toJson solo escribe lo que cambio") {
    ProjectSettings s;
    s.targetFps = 120;
    // description sigue vacia → NO debe persistirse
    const auto j = toJson(s);
    CHECK(j.contains("target_fps"));
    CHECK(j.at("target_fps") == 120);
    CHECK_FALSE(j.contains("description"));
}

TEST_CASE("ProjectSettings roundtrip preserva todos los campos") {
    ProjectSettings before;
    before.targetFps   = 144;
    before.description = "Mi proyecto piloto F3H1";

    const auto j = toJson(before);
    const auto after = projectSettingsFromJson(j);

    CHECK(after.targetFps == 144);
    CHECK(after.description == "Mi proyecto piloto F3H1");
}

TEST_CASE("ProjectSettings fromJson de empty object devuelve defaults") {
    const auto s = projectSettingsFromJson(nlohmann::json::object());
    CHECK(s.targetFps == 60);
    CHECK(s.description.empty());
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

TEST_CASE("ProjectSettings fromJson back-compat: campos faltantes → defaults") {
    // Solo target_fps presente: description queda en default (vacio).
    nlohmann::json j;
    j["target_fps"] = 30;
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps == 30);
    CHECK(s.description.empty());
}

TEST_CASE("ProjectSettings fromJson forward-compat: keys desconocidas ignoradas") {
    // Simula un .moodproj escrito por una version futura del editor
    // que agrego nuevos campos.
    nlohmann::json j;
    j["target_fps"]      = 90;
    j["description"]     = "ok";
    j["future_field_x"]  = 42;            // futuro: codigo viejo no la conoce
    j["future_field_y"]  = "anything";    // futuro: idem
    j["nested_future"]   = {{"a", 1}};    // futuro: subobject anidado

    // Debe cargar sin throw ni warning de los fields conocidos.
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps == 90);
    CHECK(s.description == "ok");
}

TEST_CASE("ProjectSettings fromJson tipo incorrecto: skip silencioso del field") {
    // target_fps esperado int pero llega string → field skipped, default.
    nlohmann::json j;
    j["target_fps"]  = "no-soy-int";
    j["description"] = 12345;            // esperado string pero llega int
    const auto s = projectSettingsFromJson(j);
    CHECK(s.targetFps == 60);            // default
    CHECK(s.description.empty());        // default
}

TEST_CASE("ProjectSettings: descripcion vacia se omite de la salida") {
    ProjectSettings s;
    s.description = "";     // explicitamente vacia
    const auto j = toJson(s);
    CHECK_FALSE(j.contains("description"));
}
