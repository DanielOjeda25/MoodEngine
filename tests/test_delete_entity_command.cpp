// Tests del DeleteEntityCommand (Hito 27 Bloque 4). Verifican el ciclo
// completo: snapshot al ctor, execute destruye, undo recrea con
// componentes preservados, y la BodyCleanup callback se invoca solo
// cuando hay RigidBodyComponent.

#include <doctest/doctest.h>

#include "editor/commands/DeleteEntityCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/assets/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <glm/vec3.hpp>

#include <memory>
#include <string>

using namespace Mood;

namespace {

class StubTexture : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory stubFactory() {
    return [](const std::string&) { return std::make_unique<StubTexture>(); };
}

usize entityCountByTag(Scene& scene, const std::string& tag) {
    usize count = 0;
    scene.forEach<TagComponent>([&](Entity, TagComponent& tc) {
        if (tc.name == tag) ++count;
    });
    return count;
}

} // namespace

TEST_CASE("DeleteEntityCommand: execute destruye la entidad") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("ToDelete");
    e.getComponent<TransformComponent>().position = glm::vec3(5, 6, 7);

    REQUIRE(entityCountByTag(scene, "ToDelete") == 1u);

    DeleteEntityCommand cmd(e, &scene, &am);
    cmd.execute();

    CHECK(entityCountByTag(scene, "ToDelete") == 0u);
}

TEST_CASE("DeleteEntityCommand: undo recrea la entidad con tag y transform") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("Recoverable");
    e.getComponent<TransformComponent>().position = glm::vec3(1.5f, 2.5f, 3.5f);
    e.getComponent<TransformComponent>().rotationEuler = glm::vec3(45.0f, 0, 0);
    e.getComponent<TransformComponent>().scale = glm::vec3(2.0f);

    DeleteEntityCommand cmd(e, &scene, &am);
    cmd.execute();
    REQUIRE(entityCountByTag(scene, "Recoverable") == 0u);

    cmd.undo();
    REQUIRE(entityCountByTag(scene, "Recoverable") == 1u);

    // Encontramos la nueva entidad y verificamos su transform.
    Entity found{};
    scene.forEach<TagComponent>([&](Entity ent, TagComponent& tc) {
        if (tc.name == "Recoverable") found = ent;
    });
    REQUIRE(static_cast<bool>(found));
    const auto& tf = found.getComponent<TransformComponent>();
    CHECK(tf.position.x == doctest::Approx(1.5f));
    CHECK(tf.position.y == doctest::Approx(2.5f));
    CHECK(tf.rotationEuler.x == doctest::Approx(45.0f));
    CHECK(tf.scale.x == doctest::Approx(2.0f));
}

TEST_CASE("DeleteEntityCommand: ciclo execute/undo/execute (redo) funciona") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("Ciclico");
    DeleteEntityCommand cmd(e, &scene, &am);

    cmd.execute();
    CHECK(entityCountByTag(scene, "Ciclico") == 0u);

    cmd.undo();
    CHECK(entityCountByTag(scene, "Ciclico") == 1u);

    cmd.execute(); // redo
    CHECK(entityCountByTag(scene, "Ciclico") == 0u);
}

TEST_CASE("DeleteEntityCommand: BodyCleanup se invoca con bodyId si hay RigidBody") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("Fisico");
    RigidBodyComponent rb;
    rb.bodyId = 42; // simulando body activo
    e.addComponent<RigidBodyComponent>(rb);

    u32 capturedBodyId = 0;
    auto cleanup = [&](u32 bodyId) { capturedBodyId = bodyId; };

    DeleteEntityCommand cmd(e, &scene, &am, cleanup);
    cmd.execute();

    CHECK(capturedBodyId == 42u);
}

TEST_CASE("DeleteEntityCommand: BodyCleanup NO se invoca si no hay RigidBody") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("SinFisica");

    bool wasCalled = false;
    auto cleanup = [&](u32) { wasCalled = true; };

    DeleteEntityCommand cmd(e, &scene, &am, cleanup);
    cmd.execute();

    CHECK_FALSE(wasCalled);
}

TEST_CASE("DeleteEntityCommand: BodyCleanup default (vacio) no crashea") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("ConFisicaSinCleanup");
    RigidBodyComponent rb;
    rb.bodyId = 99;
    e.addComponent<RigidBodyComponent>(rb);

    DeleteEntityCommand cmd(e, &scene, &am);  // sin BodyCleanup
    cmd.execute();
    // Si llegamos aca sin crashear, el null function fue manejado bien.
    CHECK(entityCountByTag(scene, "ConFisicaSinCleanup") == 0u);
}

TEST_CASE("DeleteEntityCommand via HistoryStack: integracion completa") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("ViaStack");
    e.getComponent<TransformComponent>().position = glm::vec3(7, 8, 9);

    HistoryStack h;
    h.push(std::make_unique<DeleteEntityCommand>(e, &scene, &am));
    CHECK(entityCountByTag(scene, "ViaStack") == 0u);

    REQUIRE(h.undo());
    CHECK(entityCountByTag(scene, "ViaStack") == 1u);

    REQUIRE(h.redo());
    CHECK(entityCountByTag(scene, "ViaStack") == 0u);
}

TEST_CASE("DeleteEntityCommand: name() incluye el tag") {
    Scene scene;
    AssetManager am("assets", stubFactory());
    Entity e = scene.createEntity("EntidadConNombre");
    DeleteEntityCommand cmd(e, &scene, &am);
    const auto n = cmd.name();
    CHECK(n.find("Eliminar") != std::string::npos);
    CHECK(n.find("EntidadConNombre") != std::string::npos);
}
