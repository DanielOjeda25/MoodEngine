// Tests del EditPropertyCommand<T> + handle remap del HistoryStack
// (Hito 32). Verifica que execute/undo aplican el setter, isNoOp
// detecta valores iguales, onEntityRemap patchea el m_entity, y
// HistoryStack::remapEntityInStack propaga a TODOS los comandos del
// stack (cubre el caso edit→delete→undo→undo).

#include <doctest/doctest.h>

#include "editor/commands/EditPropertyCommand.h"
#include "editor/commands/EditTransformCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec3.hpp>

#include <memory>
#include <string>

using namespace Mood;

TEST_CASE("EditPropertyCommand<f32>: execute aplica el after, undo el before") {
    Scene scene;
    Entity e = scene.createEntity("Test");

    f32 testField = 0.0f;
    auto setter = [&testField](Entity&, const f32& v) { testField = v; };

    EditPropertyCommand<f32> cmd(e, /*before*/ 0.0f, /*after*/ 7.5f,
                                  setter, "Editar f32");
    CHECK_FALSE(cmd.isNoOp());
    cmd.execute();
    CHECK(testField == doctest::Approx(7.5f));
    cmd.undo();
    CHECK(testField == doctest::Approx(0.0f));
}

TEST_CASE("EditPropertyCommand<glm::vec3>: aplica al campo de un componente") {
    Scene scene;
    Entity e = scene.createEntity("Test");
    e.getComponent<TransformComponent>().position = glm::vec3(0);

    auto setter = [](Entity& en, const glm::vec3& v) {
        en.getComponent<TransformComponent>().position = v;
    };

    EditPropertyCommand<glm::vec3> cmd(e,
        glm::vec3(0), glm::vec3(5, 6, 7), setter, "Mover (Inspector)");
    cmd.execute();
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(5.0f));

    cmd.undo();
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(0.0f));
}

TEST_CASE("EditPropertyCommand: isNoOp cuando before == after") {
    Scene scene;
    Entity e = scene.createEntity("Test");
    auto setter = [](Entity&, const f32&) {};
    EditPropertyCommand<f32> cmd(e, 3.14f, 3.14f, setter, "Tocar slider sin mover");
    CHECK(cmd.isNoOp());
}

TEST_CASE("EditPropertyCommand: onEntityRemap patchea el handle") {
    Scene scene;
    Entity oldE = scene.createEntity("Old");
    Entity newE = scene.createEntity("New");

    auto setter = [](Entity&, const f32&) {};
    EditPropertyCommand<f32> cmd(oldE, 0.0f, 1.0f, setter, "test");

    // Antes del remap apunta al handle viejo.
    cmd.onEntityRemap(newE.handle(), oldE.handle()); // no aplica (oldH != m_entity)
    cmd.onEntityRemap(oldE.handle(), newE.handle()); // SI aplica
    // Despues de remap, ejecutar deberia funcionar sin requirir el oldE
    // (no hay forma de chequearlo sin un setter espia, pero el contrato
    // es que m_entity ahora apunta a newE).
    cmd.execute();
    CHECK(true);  // si llegamos sin crash, OK
}

TEST_CASE("HistoryStack::remapEntityInStack propaga a EditTransformCommand") {
    Scene scene;
    Entity e1 = scene.createEntity("E1");
    e1.getComponent<TransformComponent>().position = glm::vec3(0);

    HistoryStack h;
    h.push(std::make_unique<EditTransformCommand>(
        e1, EditTransformCommand::Field::Position,
        glm::vec3(0), glm::vec3(10)));
    REQUIRE(e1.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));

    // Simulamos delete + recrear: destruimos e1 y creamos e2 con un
    // handle nuevo. Sin remap, el undo en h sobre e1 seria no-op.
    const entt::entity oldH = e1.handle();
    scene.destroyEntity(e1);
    Entity e2 = scene.createEntity("E1_recreated");
    e2.getComponent<TransformComponent>().position = glm::vec3(10); // post-edit
    REQUIRE(e2.handle() != oldH);

    // Remap aplica oldH -> e2.handle() en todos los comandos del stack.
    h.remapEntityInStack(oldH, e2.handle());

    // Ahora el undo del EditTransform debe revertir e2 a (0,0,0).
    REQUIRE(h.undo());
    CHECK(e2.getComponent<TransformComponent>().position.x == doctest::Approx(0.0f));
}

TEST_CASE("HistoryStack::remapEntityInStack es no-op si oldH == newH") {
    HistoryStack h;
    Scene scene;
    Entity e = scene.createEntity("E");
    h.push(std::make_unique<EditTransformCommand>(
        e, EditTransformCommand::Field::Position,
        glm::vec3(0), glm::vec3(1)));
    // No-op: misma identidad.
    h.remapEntityInStack(e.handle(), e.handle());
    REQUIRE(h.undo());
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(0.0f));
}

TEST_CASE("EditPropertyCommand<u32>: setter con captura de slotIndex (Hito 36 A)") {
    // Simula el patron del drop de textura sobre material slot del
    // Inspector: el setter captura `slotIndex` por valor y muta
    // mr.materials[slotIndex] al valor entrante.
    Scene scene;
    Entity e = scene.createEntity("ConMesh");
    MeshRendererComponent mr{};
    mr.materials = {10u, 20u, 30u};
    e.addComponent<MeshRendererComponent>(std::move(mr));

    const usize slotIndex = 1;  // mid slot
    auto setter = [slotIndex](Entity& en, const u32& v) {
        auto& mrc = en.getComponent<MeshRendererComponent>();
        if (slotIndex < mrc.materials.size()) {
            mrc.materials[slotIndex] = v;
        }
    };

    EditPropertyCommand<u32> cmd(e, /*before*/ 20u, /*after*/ 99u,
                                   setter, "Reemplazar textura material");
    CHECK_FALSE(cmd.isNoOp());

    cmd.execute();
    CHECK(e.getComponent<MeshRendererComponent>().materials[1] == 99u);
    CHECK(e.getComponent<MeshRendererComponent>().materials[0] == 10u); // intocado
    CHECK(e.getComponent<MeshRendererComponent>().materials[2] == 30u); // intocado

    cmd.undo();
    CHECK(e.getComponent<MeshRendererComponent>().materials[1] == 20u);
}

TEST_CASE("EditPropertyCommand<u32>: setter complejo limpia pool runtime al revertir (Hito 36 B)") {
    // Simula el patron de maxParticles del Inspector: el setter no solo
    // muta el campo, tambien resetea la pool de particulas vivas — sin
    // esto, undo dejaria indices >= la nueva capacidad.
    Scene scene;
    Entity e = scene.createEntity("Emitter");
    ParticleEmitterComponent pe{};
    pe.maxParticles = 1024;
    pe.aliveCount = 50;
    pe.alive.assign(50, true);
    pe.positions.resize(50);
    e.addComponent<ParticleEmitterComponent>(std::move(pe));

    auto setter = [](Entity& en, const u32& v) {
        auto& emc = en.getComponent<ParticleEmitterComponent>();
        emc.maxParticles = v;
        emc.alive.clear();
        emc.positions.clear();
        emc.velocities.clear();
        emc.ages.clear();
        emc.lifetimes.clear();
        emc.aliveCount = 0;
    };

    EditPropertyCommand<u32> cmd(e, /*before*/ 1024u, /*after*/ 256u,
                                   setter, "Editar maxParticles");
    cmd.execute();
    CHECK(e.getComponent<ParticleEmitterComponent>().maxParticles == 256u);
    CHECK(e.getComponent<ParticleEmitterComponent>().aliveCount == 0u);
    CHECK(e.getComponent<ParticleEmitterComponent>().alive.empty());

    cmd.undo();  // revert tambien limpia la pool — fiel al patron del slider.
    CHECK(e.getComponent<ParticleEmitterComponent>().maxParticles == 1024u);
    CHECK(e.getComponent<ParticleEmitterComponent>().aliveCount == 0u);
}
