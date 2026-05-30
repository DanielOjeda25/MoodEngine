// F4H2 Bloque B: tests del input bridge — parse roundtrip de UserSettings
// InputSettings y resolución case-insensitive de bindings a SDL codes.
//
// No toca SDL state real: solo verifica el contrato puro de
// `InputActions::resolveBinding` y la roundtrip JSON. La rama runtime
// (`isActionPressed` que consulta SDL_GetKeyboardState) requiere un SDL
// context activo y queda fuera del scope unit-test.

#include "core/UserSettings.h"
#include "engine/input/InputActions.h"

#include <doctest/doctest.h>
#include <nlohmann/json.hpp>

#include <SDL_scancode.h>

using namespace Mood;

TEST_CASE("InputSettings defaults: fire=mouse_left, reload=r") {
    UserSettings::InputSettings s;
    CHECK(s.keybindings.at("fire")   == "mouse_left");
    CHECK(s.keybindings.at("reload") == "r");
}

TEST_CASE("InputSettings JSON roundtrip: defaults -> empty object") {
    UserSettings::InputSettings s;
    nlohmann::json j = UserSettings::inputSettingsToJson(s);
    // Defaults: nada que persistir (mantener settings.json minimal).
    CHECK(j.empty());
}

TEST_CASE("InputSettings JSON roundtrip: edited bindings persist") {
    UserSettings::InputSettings s;
    s.keybindings["fire"]    = "mouse_right";  // override default
    s.keybindings["interact"] = "e";            // nuevo binding
    nlohmann::json j = UserSettings::inputSettingsToJson(s);
    REQUIRE(j.contains("keybindings"));
    CHECK(j["keybindings"]["fire"]     == "mouse_right");
    CHECK(j["keybindings"]["interact"] == "e");
    // Default reload=r NO se persiste.
    CHECK_FALSE(j["keybindings"].contains("reload"));

    // Roundtrip.
    UserSettings::InputSettings parsed = UserSettings::inputSettingsFromJson(j);
    CHECK(parsed.keybindings.at("fire")     == "mouse_right");
    CHECK(parsed.keybindings.at("interact") == "e");
    // Defaults siguen presentes (InputSettings ctor las setea, el parse las preserva).
    CHECK(parsed.keybindings.at("reload") == "r");
}

TEST_CASE("InputSettings fromJson: non-object -> defaults") {
    UserSettings::InputSettings s =
        UserSettings::inputSettingsFromJson(nlohmann::json::array());
    CHECK(s.keybindings.at("fire")   == "mouse_left");
    CHECK(s.keybindings.at("reload") == "r");
}

TEST_CASE("InputSettings fromJson: non-string values ignored") {
    nlohmann::json j;
    j["keybindings"]["fire"]    = "space";  // string OK
    j["keybindings"]["invalid"] = 42;        // ignorado silenciosamente
    UserSettings::InputSettings s =
        UserSettings::inputSettingsFromJson(j);
    CHECK(s.keybindings.at("fire") == "space");
    CHECK(s.keybindings.count("invalid") == 0);
}

TEST_CASE("resolveBinding: mouse buttons") {
    auto bLeft   = InputActions::resolveBinding("mouse_left");
    CHECK(bLeft.type == InputActions::BindingType::MouseButton);
    auto bRight  = InputActions::resolveBinding("mouse_right");
    CHECK(bRight.type == InputActions::BindingType::MouseButton);
    auto bMiddle = InputActions::resolveBinding("mouse_middle");
    CHECK(bMiddle.type == InputActions::BindingType::MouseButton);
    CHECK(bLeft.code != bRight.code);
}

TEST_CASE("resolveBinding: letters a-z mapean a SDL_SCANCODE_X") {
    auto bA = InputActions::resolveBinding("a");
    CHECK(bA.type == InputActions::BindingType::Key);
    CHECK(bA.code == SDL_SCANCODE_A);
    auto bR = InputActions::resolveBinding("r");
    CHECK(bR.type == InputActions::BindingType::Key);
    CHECK(bR.code == SDL_SCANCODE_R);
    auto bZ = InputActions::resolveBinding("z");
    CHECK(bZ.code == SDL_SCANCODE_Z);
}

TEST_CASE("resolveBinding: case-insensitive + trim whitespace") {
    auto a = InputActions::resolveBinding("R");
    auto b = InputActions::resolveBinding("r");
    auto c = InputActions::resolveBinding(" r ");
    CHECK(a.code == b.code);
    CHECK(b.code == c.code);

    auto mUpper = InputActions::resolveBinding("MOUSE_LEFT");
    auto mLower = InputActions::resolveBinding("mouse_left");
    CHECK(mUpper.code == mLower.code);
    CHECK(mUpper.type == mLower.type);
}

TEST_CASE("resolveBinding: special keys (space, escape, lshift)") {
    CHECK(InputActions::resolveBinding("space").code   == SDL_SCANCODE_SPACE);
    CHECK(InputActions::resolveBinding("escape").code  == SDL_SCANCODE_ESCAPE);
    CHECK(InputActions::resolveBinding("lshift").code  == SDL_SCANCODE_LSHIFT);
    CHECK(InputActions::resolveBinding("enter").code   == SDL_SCANCODE_RETURN);
}

TEST_CASE("resolveBinding: unknown string -> None") {
    auto b = InputActions::resolveBinding("foobar");
    CHECK(b.type == InputActions::BindingType::None);
    auto empty = InputActions::resolveBinding("");
    CHECK(empty.type == InputActions::BindingType::None);
    auto multi = InputActions::resolveBinding("ab");
    CHECK(multi.type == InputActions::BindingType::None);
}
