// Tests del CreateEntityCommand (Hito 28). Verifican el ciclo simetrico
// a DeleteEntityCommand: el primer execute() es no-op (la entidad ya fue
// creada por el callsite), undo() destruye, y un siguiente execute()
// (redo) recrea desde el snapshot. Soporta multiples entidades en un
// solo comando + cleanup de bodies de Jolt.

#include <doctest/doctest.h>

#include "editor/commands/CreateEntityCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec3.hpp>

#include <memory>
#include <string>

using namespace Mood;

namespace {

class StubTextureCreate : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory stubFactoryCreate() {
    return [](const std::string&) { return std::make_unique<StubTextureCreate>(); };
}

usize entityCountByTagCreate(Scene& scene, const std::string& tag) {
    usize count = 0;
    scene.forEach<TagComponent>([&](Entity, TagComponent& tc) {
        if (tc.name == tag) ++count;
    });
    return count;
}

} // namespace

TEST_CASE("CreateEntityCommand: primer execute() no destruye (la entidad ya existe)") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());
    Entity e = scene.createEntity("Spawneada");
    e.getComponent<TransformComponent>().position = glm::vec3(1, 2, 3);

    CreateEntityCommand cmd({e}, &scene, &am, {}, "Spawn dummy");
    cmd.execute();  // primer execute = no-op

    CHECK(entityCountByTagCreate(scene, "Spawneada") == 1u);
}

TEST_CASE("CreateEntityCommand: undo destruye + redo recrea con transform preservado") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());
    Entity e = scene.createEntity("Rebote");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position      = glm::vec3(4.5f, 5.5f, 6.5f);
    tf.rotationEuler = glm::vec3(0, 90.0f, 0);
    tf.scale         = glm::vec3(3.0f);

    CreateEntityCommand cmd({e}, &scene, &am, {}, "Spawn rebote");

    cmd.undo();  // destroy
    CHECK(entityCountByTagCreate(scene, "Rebote") == 0u);

    cmd.execute();  // redo (recrea)
    REQUIRE(entityCountByTagCreate(scene, "Rebote") == 1u);

    Entity found{};
    scene.forEach<TagComponent>([&](Entity ent, TagComponent& tc) {
        if (tc.name == "Rebote") found = ent;
    });
    REQUIRE(static_cast<bool>(found));
    const auto& nt = found.getComponent<TransformComponent>();
    CHECK(nt.position.x == doctest::Approx(4.5f));
    CHECK(nt.rotationEuler.y == doctest::Approx(90.0f));
    CHECK(nt.scale.x == doctest::Approx(3.0f));
}

TEST_CASE("CreateEntityCommand: snapshot de varias entidades, undo borra todas") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());

    std::vector<Entity> created;
    for (int i = 0; i < 5; ++i) {
        Entity e = scene.createEntity("Multi");
        created.push_back(e);
    }
    REQUIRE(entityCountByTagCreate(scene, "Multi") == 5u);

    CreateEntityCommand cmd(created, &scene, &am, {}, "Spawn varios");
    cmd.undo();
    CHECK(entityCountByTagCreate(scene, "Multi") == 0u);

    cmd.execute(); // redo
    CHECK(entityCountByTagCreate(scene, "Multi") == 5u);
}

TEST_CASE("CreateEntityCommand: ciclo undo/execute/undo/execute consistente") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());
    Entity e = scene.createEntity("Ping");
    CreateEntityCommand cmd({e}, &scene, &am, {}, "Spawn ping");

    cmd.undo();
    CHECK(entityCountByTagCreate(scene, "Ping") == 0u);
    cmd.execute();
    CHECK(entityCountByTagCreate(scene, "Ping") == 1u);
    cmd.undo();
    CHECK(entityCountByTagCreate(scene, "Ping") == 0u);
    cmd.execute();
    CHECK(entityCountByTagCreate(scene, "Ping") == 1u);
}

TEST_CASE("CreateEntityCommand: BodyCleanup se invoca al undo si hay RigidBody") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());
    Entity e = scene.createEntity("ConFisica");
    RigidBodyComponent rb;
    rb.bodyId = 77;
    e.addComponent<RigidBodyComponent>(rb);

    u32 captured = 0;
    auto cleanup = [&](u32 id) { captured = id; };

    CreateEntityCommand cmd({e}, &scene, &am, cleanup, "Spawn fisico");
    cmd.undo();

    CHECK(captured == 77u);
}

TEST_CASE("CreateEntityCommand: lista vacia es no-op") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());

    CreateEntityCommand cmd(std::vector<Entity>{}, &scene, &am, {}, "Spawn vacio");
    cmd.undo();      // no debe crashear
    cmd.execute();   // no debe crashear
    CHECK(cmd.name() == "Spawn vacio");
}

TEST_CASE("CreateEntityCommand via HistoryStack: integracion") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());
    Entity e = scene.createEntity("ViaHistory");
    e.getComponent<TransformComponent>().position = glm::vec3(10, 20, 30);

    HistoryStack h;
    h.push(std::make_unique<CreateEntityCommand>(
        std::vector<Entity>{e}, &scene, &am, CreateEntityCommand::BodyCleanup{},
        "Spawn via stack"));
    // El push llama execute() — pero en CreateEntityCommand el primer
    // execute es no-op, asi que la entidad sigue viva.
    CHECK(entityCountByTagCreate(scene, "ViaHistory") == 1u);

    REQUIRE(h.undo());
    CHECK(entityCountByTagCreate(scene, "ViaHistory") == 0u);

    REQUIRE(h.redo());
    CHECK(entityCountByTagCreate(scene, "ViaHistory") == 1u);
}

TEST_CASE("CreateEntityCommand: name() devuelve el label dado") {
    Scene scene;
    AssetManager am("assets", stubFactoryCreate());
    Entity e = scene.createEntity("Etiqueta");
    CreateEntityCommand cmd({e}, &scene, &am, {}, "Spawn enemigo demo");
    CHECK(cmd.name() == "Spawn enemigo demo");
}
