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

// ============================================================
// F4H3 — scroll wheel + numero + scroll delta state
// ============================================================

TEST_CASE("F4H3 resolveBinding: mouse_wheel_up -> MouseWheel +1") {
    auto b = InputActions::resolveBinding("mouse_wheel_up");
    CHECK(b.type == InputActions::BindingType::MouseWheel);
    CHECK(b.code == +1);
}

TEST_CASE("F4H3 resolveBinding: mouse_wheel_down -> MouseWheel -1") {
    auto b = InputActions::resolveBinding("mouse_wheel_down");
    CHECK(b.type == InputActions::BindingType::MouseWheel);
    CHECK(b.code == -1);
}

TEST_CASE("F4H3 resolveBinding: digitos 1-9 -> SDL_SCANCODE_1..9") {
    CHECK(InputActions::resolveBinding("1").code == SDL_SCANCODE_1);
    CHECK(InputActions::resolveBinding("2").code == SDL_SCANCODE_2);
    CHECK(InputActions::resolveBinding("4").code == SDL_SCANCODE_4);
    CHECK(InputActions::resolveBinding("9").code == SDL_SCANCODE_9);
}

TEST_CASE("F4H3 resolveBinding: digito 0 -> SDL_SCANCODE_0") {
    CHECK(InputActions::resolveBinding("0").code == SDL_SCANCODE_0);
}

TEST_CASE("F4H3 InputSettings defaults: weapon_next, weapon_prev, weapon_last, weapon_1..4") {
    UserSettings::InputSettings s;
    CHECK(s.keybindings.at("weapon_next") == "mouse_wheel_up");
    CHECK(s.keybindings.at("weapon_prev") == "mouse_wheel_down");
    CHECK(s.keybindings.at("weapon_last") == "q");
    CHECK(s.keybindings.at("weapon_1") == "1");
    CHECK(s.keybindings.at("weapon_2") == "2");
    CHECK(s.keybindings.at("weapon_3") == "3");
    CHECK(s.keybindings.at("weapon_4") == "4");
}

TEST_CASE("F4H3 scroll delta: notifyScrollEvent + pollScrollDelta acumula + consume") {
    InputActions::resetState();
    CHECK(InputActions::pollScrollDelta() == 0);

    InputActions::notifyScrollEvent(+1);
    InputActions::notifyScrollEvent(+2);
    CHECK(InputActions::pollScrollDelta() == +3);
    // poll consume — siguiente poll devuelve 0.
    CHECK(InputActions::pollScrollDelta() == 0);

    InputActions::notifyScrollEvent(-5);
    CHECK(InputActions::pollScrollDelta() == -5);
}

TEST_CASE("F4H3 endFrame: resetea scroll delta") {
    InputActions::resetState();
    InputActions::notifyScrollEvent(+7);
    InputActions::endFrame();
    CHECK(InputActions::pollScrollDelta() == 0);
}
