// Tests del MultiEditPropertyCommand<T> (F3H8). Snapshot semantics:
// cada Entry guarda su before individual, TODAS reciben el mismo after.
// Undo restaura cada una a su before. Skip silencioso si la entidad
// fue destruida tras el push.

#include <doctest/doctest.h>

#include "editor/commands/MultiEditPropertyCommand.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec3.hpp>

#include <vector>

using namespace Mood;

TEST_CASE("MultiEditPropertyCommand<f32>: 1 entity == EditPropertyCommand simple") {
    Scene scene;
    Entity e = scene.createEntity("A");
    e.getComponent<TransformComponent>().position.x = 1.0f;

    auto setter = [](Entity& en, const f32& v) {
        en.getComponent<TransformComponent>().position.x = v;
    };

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({e, /*before=*/ 1.0f});

    MultiEditPropertyCommand<f32> cmd(std::move(entries), /*after=*/ 5.0f,
                                        setter, "Multi-edit");
    CHECK(cmd.entryCount() == 1u);
    CHECK_FALSE(cmd.isNoOp());

    cmd.execute();
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(5.0f));
    cmd.undo();
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(1.0f));
}

TEST_CASE("MultiEditPropertyCommand<f32>: N entities con mismo before, mismo after") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    Entity c = scene.createEntity("C");
    a.getComponent<TransformComponent>().position.x = 2.0f;
    b.getComponent<TransformComponent>().position.x = 2.0f;
    c.getComponent<TransformComponent>().position.x = 2.0f;

    auto setter = [](Entity& en, const f32& v) {
        en.getComponent<TransformComponent>().position.x = v;
    };

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({a, 2.0f});
    entries.push_back({b, 2.0f});
    entries.push_back({c, 2.0f});

    MultiEditPropertyCommand<f32> cmd(std::move(entries), 10.0f, setter,
                                        "Multi-edit");
    cmd.execute();
    CHECK(a.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));
    CHECK(b.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));
    CHECK(c.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));

    cmd.undo();
    CHECK(a.getComponent<TransformComponent>().position.x == doctest::Approx(2.0f));
    CHECK(b.getComponent<TransformComponent>().position.x == doctest::Approx(2.0f));
    CHECK(c.getComponent<TransformComponent>().position.x == doctest::Approx(2.0f));
}

TEST_CASE("MultiEditPropertyCommand<f32>: N entities con befores distintos (mixed)") {
    // Caso real: dev selecciona 3 luces con intensities distintas, mueve el
    // slider a 10. Execute homogeniza a 10. Undo restaura los 3 distintos.
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    Entity c = scene.createEntity("C");
    a.getComponent<TransformComponent>().position.x = 1.0f;
    b.getComponent<TransformComponent>().position.x = 2.0f;
    c.getComponent<TransformComponent>().position.x = 3.0f;

    auto setter = [](Entity& en, const f32& v) {
        en.getComponent<TransformComponent>().position.x = v;
    };

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({a, 1.0f});
    entries.push_back({b, 2.0f});
    entries.push_back({c, 3.0f});

    MultiEditPropertyCommand<f32> cmd(std::move(entries), 10.0f, setter,
                                        "Multi-edit");
    CHECK_FALSE(cmd.isNoOp());
    cmd.execute();
    CHECK(a.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));
    CHECK(b.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));
    CHECK(c.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));

    cmd.undo();
    CHECK(a.getComponent<TransformComponent>().position.x == doctest::Approx(1.0f));
    CHECK(b.getComponent<TransformComponent>().position.x == doctest::Approx(2.0f));
    CHECK(c.getComponent<TransformComponent>().position.x == doctest::Approx(3.0f));
}

TEST_CASE("MultiEditPropertyCommand<vec3>: color de 3 luces") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    a.addComponent<LightComponent>();
    b.addComponent<LightComponent>();
    a.getComponent<LightComponent>().color = glm::vec3(1.0f, 0.0f, 0.0f);
    b.getComponent<LightComponent>().color = glm::vec3(0.0f, 1.0f, 0.0f);

    auto setter = [](Entity& en, const glm::vec3& v) {
        if (en.hasComponent<LightComponent>())
            en.getComponent<LightComponent>().color = v;
    };

    std::vector<MultiEditPropertyCommand<glm::vec3>::Entry> entries;
    entries.push_back({a, glm::vec3(1, 0, 0)});
    entries.push_back({b, glm::vec3(0, 1, 0)});

    MultiEditPropertyCommand<glm::vec3> cmd(std::move(entries),
                                                glm::vec3(1, 1, 1), setter,
                                                "Multi-color");
    cmd.execute();
    CHECK(a.getComponent<LightComponent>().color.x == doctest::Approx(1.0f));
    CHECK(a.getComponent<LightComponent>().color.y == doctest::Approx(1.0f));
    CHECK(b.getComponent<LightComponent>().color.x == doctest::Approx(1.0f));
    CHECK(b.getComponent<LightComponent>().color.y == doctest::Approx(1.0f));

    cmd.undo();
    CHECK(a.getComponent<LightComponent>().color.x == doctest::Approx(1.0f));
    CHECK(a.getComponent<LightComponent>().color.y == doctest::Approx(0.0f));
    CHECK(b.getComponent<LightComponent>().color.x == doctest::Approx(0.0f));
    CHECK(b.getComponent<LightComponent>().color.y == doctest::Approx(1.0f));
}

TEST_CASE("MultiEditPropertyCommand: isNoOp cuando todos los befores == after") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    auto setter = [](Entity&, const f32&) {};

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({a, 5.0f});
    entries.push_back({b, 5.0f});

    MultiEditPropertyCommand<f32> cmd(std::move(entries), 5.0f, setter,
                                        "NoOp test");
    CHECK(cmd.isNoOp());
}

TEST_CASE("MultiEditPropertyCommand: isNoOp false si AL MENOS UNO difiere") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    auto setter = [](Entity&, const f32&) {};

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({a, 5.0f});  // before == after
    entries.push_back({b, 3.0f});  // before != after

    MultiEditPropertyCommand<f32> cmd(std::move(entries), 5.0f, setter,
                                        "Mixed-noop");
    CHECK_FALSE(cmd.isNoOp());
}

TEST_CASE("MultiEditPropertyCommand: skip silencioso de entidades destruidas") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    a.getComponent<TransformComponent>().position.x = 1.0f;
    b.getComponent<TransformComponent>().position.x = 2.0f;

    auto setter = [](Entity& en, const f32& v) {
        en.getComponent<TransformComponent>().position.x = v;
    };

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({a, 1.0f});
    entries.push_back({b, 2.0f});

    MultiEditPropertyCommand<f32> cmd(std::move(entries), 10.0f, setter,
                                        "Skip-destroyed");

    // Destruir b ANTES del execute. La b entry queda invalida pero
    // execute no debe crashear.
    scene.destroyEntity(b);

    cmd.execute();
    CHECK(a.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));
    // b ya no existe — no se chequea (destroyEntity invalida el handle).

    cmd.undo();
    CHECK(a.getComponent<TransformComponent>().position.x == doctest::Approx(1.0f));
}

TEST_CASE("MultiEditPropertyCommand: redo idempotente") {
    Scene scene;
    Entity a = scene.createEntity("A");
    Entity b = scene.createEntity("B");
    a.getComponent<TransformComponent>().position.x = 1.0f;
    b.getComponent<TransformComponent>().position.x = 2.0f;

    auto setter = [](Entity& en, const f32& v) {
        en.getComponent<TransformComponent>().position.x = v;
    };

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({a, 1.0f});
    entries.push_back({b, 2.0f});

    MultiEditPropertyCommand<f32> cmd(std::move(entries), 99.0f, setter,
                                        "Redo-test");
    cmd.execute();
    cmd.undo();
    cmd.execute();
    cmd.execute();  // redo idempotente — mismo state
    CHECK(a.getComponent<TransformComponent>().position.x == doctest::Approx(99.0f));
    CHECK(b.getComponent<TransformComponent>().position.x == doctest::Approx(99.0f));
}

TEST_CASE("MultiEditPropertyCommand: onEntityRemap patchea el handle") {
    Scene scene;
    Entity a = scene.createEntity("A");
    a.getComponent<TransformComponent>().position.x = 1.0f;

    auto setter = [](Entity& en, const f32& v) {
        en.getComponent<TransformComponent>().position.x = v;
    };

    std::vector<MultiEditPropertyCommand<f32>::Entry> entries;
    entries.push_back({a, 1.0f});

    MultiEditPropertyCommand<f32> cmd(std::move(entries), 10.0f, setter,
                                        "Remap-test");

    // Simular un remap: destruir + recrear, llamar onEntityRemap.
    const entt::entity oldH = a.handle();
    scene.destroyEntity(a);
    Entity a2 = scene.createEntity("A_recreated");
    cmd.onEntityRemap(oldH, a2.handle());
    a2.getComponent<TransformComponent>().position.x = 1.0f;

    cmd.execute();
    CHECK(a2.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));
}
