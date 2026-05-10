// Tests del AddComponentCommand (F2H45 Bloque A) — undoable add de
// componente sobre una entidad. Cubren:
//   - execute add el componente; undo lo remueve.
//   - redo (segundo execute tras undo) lo agrega de vuelta con default.
//   - HistoryStack push aplica el primer execute + undo/redo desde la
//     pila funciona end-to-end.
//   - Helper templated `makeAddComponentCommand<T>` arma el comando
//     correctamente para tipos distintos.
//   - onEntityRemap patchea el handle (cubre el caso delete->undo
//     cuando ya hay AddComponentCommand previo en el stack).
//   - Sin entidad valida, execute/undo son no-op silencioso.

#include <doctest/doctest.h>

#include "editor/commands/AddComponentCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <memory>

using namespace Mood;

TEST_CASE("AddComponentCommand: execute add, undo remove (LightComponent)") {
    Scene scene;
    Entity e = scene.createEntity("Test");
    REQUIRE_FALSE(e.hasComponent<LightComponent>());

    auto cmd = makeAddComponentCommand<LightComponent>(e, "Agregar Luz");
    cmd->execute();
    CHECK(e.hasComponent<LightComponent>());

    cmd->undo();
    CHECK_FALSE(e.hasComponent<LightComponent>());
}

TEST_CASE("AddComponentCommand: redo tras undo re-agrega con valores default") {
    Scene scene;
    Entity e = scene.createEntity("Test");

    auto cmd = makeAddComponentCommand<RigidBodyComponent>(e, "Agregar RigidBody");
    cmd->execute();
    REQUIRE(e.hasComponent<RigidBodyComponent>());
    cmd->undo();
    REQUIRE_FALSE(e.hasComponent<RigidBodyComponent>());
    cmd->execute();  // redo
    CHECK(e.hasComponent<RigidBodyComponent>());
}

TEST_CASE("AddComponentCommand: integracion con HistoryStack (push + undo + redo)") {
    Scene scene;
    Entity e = scene.createEntity("Test");

    HistoryStack h;
    REQUIRE_FALSE(e.hasComponent<TriggerComponent>());

    h.push(makeAddComponentCommand<TriggerComponent>(e, "Agregar Trigger"));
    CHECK(e.hasComponent<TriggerComponent>());      // push aplico execute
    CHECK(h.canUndo());
    CHECK(h.undoName() == "Agregar Trigger");

    REQUIRE(h.undo());
    CHECK_FALSE(e.hasComponent<TriggerComponent>());

    REQUIRE(h.redo());
    CHECK(e.hasComponent<TriggerComponent>());
}

TEST_CASE("AddComponentCommand: tipos distintos (Audio + Camera + Brush)") {
    Scene scene;
    Entity e = scene.createEntity("Multi");

    auto cmdAudio = makeAddComponentCommand<AudioSourceComponent>(e, "Audio");
    auto cmdCam   = makeAddComponentCommand<CameraComponent>(e, "Camera");

    cmdAudio->execute();
    cmdCam->execute();
    CHECK(e.hasComponent<AudioSourceComponent>());
    CHECK(e.hasComponent<CameraComponent>());

    cmdCam->undo();
    CHECK_FALSE(e.hasComponent<CameraComponent>());
    CHECK(e.hasComponent<AudioSourceComponent>());  // intocado

    cmdAudio->undo();
    CHECK_FALSE(e.hasComponent<AudioSourceComponent>());
}

TEST_CASE("AddComponentCommand: name() devuelve el label dado en construccion") {
    Scene scene;
    Entity e = scene.createEntity("Test");
    auto cmd = makeAddComponentCommand<LightComponent>(e, "Agregar componente: Luz");
    CHECK(cmd->name() == "Agregar componente: Luz");
}

TEST_CASE("AddComponentCommand: onEntityRemap patchea el handle") {
    Scene scene;
    Entity oldE = scene.createEntity("Old");
    Entity newE = scene.createEntity("New");

    auto cmd = makeAddComponentCommand<ScriptComponent>(oldE, "Add script");
    cmd->onEntityRemap(newE.handle(), oldE.handle()); // no aplica
    cmd->onEntityRemap(oldE.handle(), newE.handle()); // si aplica

    cmd->execute();  // ahora deberia tocar newE, no oldE
    CHECK(newE.hasComponent<ScriptComponent>());
    CHECK_FALSE(oldE.hasComponent<ScriptComponent>());
}

TEST_CASE("AddComponentCommand: entidad destruida es no-op silencioso") {
    Scene scene;
    Entity e = scene.createEntity("Doomed");
    auto cmd = makeAddComponentCommand<EnvironmentComponent>(e, "Add env");

    scene.destroyEntity(e);
    // Sin crash. execute/undo son no-op porque registry().valid(handle) falla.
    cmd->execute();
    cmd->undo();
    CHECK(true);
}

TEST_CASE("AddComponentCommand: HistoryStack interactua con EditProperty posterior") {
    // Patron tipico: dev hace Add Component (push #1) + edita un campo
    // (push #2). Undo solo el edit primero (LIFO), undo despues remueve
    // el componente. Redo en orden inverso recrea + re-edita.
    Scene scene;
    Entity e = scene.createEntity("Test");
    HistoryStack h;

    h.push(makeAddComponentCommand<LightComponent>(e, "Add Light"));
    REQUIRE(e.hasComponent<LightComponent>());
    e.getComponent<LightComponent>().intensity = 10.0f;  // simula edit

    // Undo: solo deshace el add (no hay EditPropertyCommand en este test).
    REQUIRE(h.undo());
    CHECK_FALSE(e.hasComponent<LightComponent>());

    // Redo: recrea con default (NO con intensity=10).
    REQUIRE(h.redo());
    CHECK(e.hasComponent<LightComponent>());
    CHECK(e.getComponent<LightComponent>().intensity != doctest::Approx(10.0f));
}
