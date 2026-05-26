// F3H12: tests de regresion para los nuevos tipos cubiertos por
// EditPropertyCommand<T> y MultiEditPropertyCommand<T> tras el undo
// coverage audit del Inspector. Pre-F3H12 el variant del
// MultiEditTracker solo soportaba f32 y glm::vec3; F3H12 anade
// bool, u32, glm::vec4. Tests verifican round-trip execute/undo
// para los nuevos tipos.
//
// Cobertura indirecta: los setters de los renderXxxSection del
// Inspector usan EditPropertyCommand<bool/u32/std::string/vec4>;
// estos tests validan que la mecanica execute/undo es correcta
// para esos tipos. Tests de UI (toggle + Ctrl+Z) viven en
// validacion visual — fuera del scope de doctest.

#include <doctest/doctest.h>

#include "editor/commands/EditPropertyCommand.h"
#include "editor/commands/MultiEditPropertyCommand.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec4.hpp>

#include <string>
#include <vector>

using namespace Mood;

// === EditPropertyCommand<bool> — Light.enabled, Trigger.oneShot, etc. ====

TEST_CASE("F3H12 EditPropertyCommand<bool>: toggle round-trip") {
    Scene scene;
    Entity e = scene.createEntity("Light");
    e.addComponent<LightComponent>();
    e.getComponent<LightComponent>().enabled = true;

    auto setter = [](Entity& en, const bool& v) {
        en.getComponent<LightComponent>().enabled = v;
    };

    EditPropertyCommand<bool> cmd(e, /*before=*/ true, /*after=*/ false,
                                    setter, "Toggle light enabled");
    CHECK_FALSE(cmd.isNoOp());
    cmd.execute();
    CHECK(e.getComponent<LightComponent>().enabled == false);
    cmd.undo();
    CHECK(e.getComponent<LightComponent>().enabled == true);
}

TEST_CASE("F3H12 EditPropertyCommand<bool>: isNoOp cuando no cambia") {
    Scene scene;
    Entity e = scene.createEntity("X");
    auto setter = [](Entity&, const bool&) {};
    EditPropertyCommand<bool> cmd(e, true, true, setter, "no-op");
    CHECK(cmd.isNoOp());
}

// === EditPropertyCommand<u32> — combos (enums + asset ids) =================

TEST_CASE("F3H12 EditPropertyCommand<u32>: enum combo round-trip") {
    Scene scene;
    Entity e = scene.createEntity("Light");
    e.addComponent<LightComponent>();
    auto& lt = e.getComponent<LightComponent>();
    lt.type = LightComponent::Type::Directional;

    auto setter = [](Entity& en, const u32& v) {
        en.getComponent<LightComponent>().type =
            static_cast<LightComponent::Type>(v);
    };

    EditPropertyCommand<u32> cmd(e,
        /*before=*/ static_cast<u32>(LightComponent::Type::Directional),
        /*after=*/  static_cast<u32>(LightComponent::Type::Point),
        setter, "Cambiar light type");
    cmd.execute();
    CHECK(lt.type == LightComponent::Type::Point);
    cmd.undo();
    CHECK(lt.type == LightComponent::Type::Directional);
}

// === EditPropertyCommand<std::string> — Script.path, paths del Inspector ==

TEST_CASE("F3H12 EditPropertyCommand<std::string>: path round-trip") {
    Scene scene;
    Entity e = scene.createEntity("Script");
    e.addComponent<ScriptComponent>();
    auto& sc = e.getComponent<ScriptComponent>();
    sc.path = "scripts/old.lua";

    auto setter = [](Entity& en, const std::string& v) {
        en.getComponent<ScriptComponent>().path = v;
    };

    EditPropertyCommand<std::string> cmd(e,
        std::string("scripts/old.lua"),
        std::string("scripts/new.lua"),
        setter, "Editar script path");
    cmd.execute();
    CHECK(sc.path == "scripts/new.lua");
    cmd.undo();
    CHECK(sc.path == "scripts/old.lua");
}

// === MultiEditPropertyCommand<bool> — multi-toggle de N entidades ==========

TEST_CASE("F3H12 MultiEditPropertyCommand<bool>: N lights enabled toggle") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    Entity c = scene.createEntity("C");
    a.addComponent<LightComponent>();
    b.addComponent<LightComponent>();
    c.addComponent<LightComponent>();
    a.getComponent<LightComponent>().enabled = true;
    b.getComponent<LightComponent>().enabled = false;  // mixed before
    c.getComponent<LightComponent>().enabled = true;

    auto setter = [](Entity& en, const bool& v) {
        if (en.hasComponent<LightComponent>()) {
            en.getComponent<LightComponent>().enabled = v;
        }
    };

    std::vector<MultiEditPropertyCommand<bool>::Entry> entries;
    entries.push_back({a, /*before=*/ true});
    entries.push_back({b, /*before=*/ false});
    entries.push_back({c, /*before=*/ true});

    MultiEditPropertyCommand<bool> cmd(std::move(entries), /*after=*/ false,
                                         setter, "Multi-toggle");
    CHECK(cmd.entryCount() == 3u);
    CHECK_FALSE(cmd.isNoOp());

    cmd.execute();
    // Todas reciben el after homogeneo.
    CHECK(a.getComponent<LightComponent>().enabled == false);
    CHECK(b.getComponent<LightComponent>().enabled == false);
    CHECK(c.getComponent<LightComponent>().enabled == false);

    cmd.undo();
    // Cada una vuelve a su before individual.
    CHECK(a.getComponent<LightComponent>().enabled == true);
    CHECK(b.getComponent<LightComponent>().enabled == false);
    CHECK(c.getComponent<LightComponent>().enabled == true);
}

// === MultiEditPropertyCommand<u32> — combo en N entidades =================

TEST_CASE("F3H12 MultiEditPropertyCommand<u32>: N lights type combo") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    a.addComponent<LightComponent>();
    b.addComponent<LightComponent>();
    a.getComponent<LightComponent>().type = LightComponent::Type::Directional;
    b.getComponent<LightComponent>().type = LightComponent::Type::Point;

    auto setter = [](Entity& en, const u32& v) {
        if (en.hasComponent<LightComponent>()) {
            en.getComponent<LightComponent>().type =
                static_cast<LightComponent::Type>(v);
        }
    };

    std::vector<MultiEditPropertyCommand<u32>::Entry> entries;
    entries.push_back({a, static_cast<u32>(LightComponent::Type::Directional)});
    entries.push_back({b, static_cast<u32>(LightComponent::Type::Point)});

    MultiEditPropertyCommand<u32> cmd(std::move(entries),
        /*after=*/ static_cast<u32>(LightComponent::Type::Point),
        setter, "Multi-combo");

    cmd.execute();
    CHECK(a.getComponent<LightComponent>().type == LightComponent::Type::Point);
    CHECK(b.getComponent<LightComponent>().type == LightComponent::Type::Point);

    cmd.undo();
    CHECK(a.getComponent<LightComponent>().type == LightComponent::Type::Directional);
    CHECK(b.getComponent<LightComponent>().type == LightComponent::Type::Point);
}

// === MultiEditPropertyCommand<glm::vec4> — ColorEdit4 en N particles ======

TEST_CASE("F3H12 MultiEditPropertyCommand<glm::vec4>: N particle colorStart") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    a.addComponent<ParticleEmitterComponent>();
    b.addComponent<ParticleEmitterComponent>();
    const glm::vec4 redA(1.0f, 0.0f, 0.0f, 1.0f);
    const glm::vec4 greenB(0.0f, 1.0f, 0.0f, 1.0f);
    a.getComponent<ParticleEmitterComponent>().colorStart = redA;
    b.getComponent<ParticleEmitterComponent>().colorStart = greenB;

    auto setter = [](Entity& en, const glm::vec4& v) {
        if (en.hasComponent<ParticleEmitterComponent>()) {
            en.getComponent<ParticleEmitterComponent>().colorStart = v;
        }
    };

    std::vector<MultiEditPropertyCommand<glm::vec4>::Entry> entries;
    entries.push_back({a, redA});
    entries.push_back({b, greenB});

    const glm::vec4 blue(0.0f, 0.0f, 1.0f, 1.0f);
    MultiEditPropertyCommand<glm::vec4> cmd(std::move(entries), blue,
                                              setter, "Multi-color");

    cmd.execute();
    CHECK(a.getComponent<ParticleEmitterComponent>().colorStart == blue);
    CHECK(b.getComponent<ParticleEmitterComponent>().colorStart == blue);

    cmd.undo();
    CHECK(a.getComponent<ParticleEmitterComponent>().colorStart == redA);
    CHECK(b.getComponent<ParticleEmitterComponent>().colorStart == greenB);
}
