// Tests del EditTransformCommand (Hito 27 Bloque 2). Verifican que
// execute/undo aplican el valor correcto al Transform de la entidad
// y que la deteccion de no-op + entidad destruida funcionan.

#include <doctest/doctest.h>

#include "editor/commands/EditTransformCommand.h"
#include "editor/commands/HistoryStack.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec3.hpp>

#include <memory>

using namespace Mood;

namespace {

Entity setupEntity(Scene& scene, const glm::vec3& pos = {1, 2, 3}) {
    Entity e = scene.createEntity("test");
    auto& tf = e.getComponent<TransformComponent>();
    tf.position = pos;
    tf.rotationEuler = glm::vec3(10.0f, 20.0f, 30.0f);
    tf.scale = glm::vec3(2.0f);
    return e;
}

} // namespace

TEST_CASE("EditTransformCommand: Position execute aplica el after") {
    Scene scene;
    Entity e = setupEntity(scene, glm::vec3(0));
    EditTransformCommand cmd(e, EditTransformCommand::Field::Position,
                              glm::vec3(0), glm::vec3(5, 6, 7));
    cmd.execute();
    const auto& tf = e.getComponent<TransformComponent>();
    CHECK(tf.position.x == doctest::Approx(5.0f));
    CHECK(tf.position.y == doctest::Approx(6.0f));
    CHECK(tf.position.z == doctest::Approx(7.0f));
}

TEST_CASE("EditTransformCommand: undo revierte al before") {
    Scene scene;
    Entity e = setupEntity(scene, glm::vec3(1, 2, 3));
    EditTransformCommand cmd(e, EditTransformCommand::Field::Position,
                              glm::vec3(1, 2, 3), glm::vec3(99, 99, 99));
    cmd.execute();
    cmd.undo();
    const auto& tf = e.getComponent<TransformComponent>();
    CHECK(tf.position.x == doctest::Approx(1.0f));
    CHECK(tf.position.y == doctest::Approx(2.0f));
    CHECK(tf.position.z == doctest::Approx(3.0f));
}

TEST_CASE("EditTransformCommand: Rotation field afecta solo rotationEuler") {
    Scene scene;
    Entity e = setupEntity(scene);
    const glm::vec3 posBefore = e.getComponent<TransformComponent>().position;
    EditTransformCommand cmd(e, EditTransformCommand::Field::Rotation,
                              glm::vec3(0), glm::vec3(45, 90, 0));
    cmd.execute();
    const auto& tf = e.getComponent<TransformComponent>();
    CHECK(tf.rotationEuler.x == doctest::Approx(45.0f));
    CHECK(tf.rotationEuler.y == doctest::Approx(90.0f));
    // Position no debe haberse tocado.
    CHECK(tf.position == posBefore);
}

TEST_CASE("EditTransformCommand: Scale field afecta solo scale") {
    Scene scene;
    Entity e = setupEntity(scene);
    EditTransformCommand cmd(e, EditTransformCommand::Field::Scale,
                              glm::vec3(2.0f), glm::vec3(10.0f));
    cmd.execute();
    const auto& tf = e.getComponent<TransformComponent>();
    CHECK(tf.scale.x == doctest::Approx(10.0f));
    // Rotation/Position intactos.
    CHECK(tf.rotationEuler.x == doctest::Approx(10.0f));
}

TEST_CASE("EditTransformCommand: isNoOp cuando before == after") {
    Scene scene;
    Entity e = setupEntity(scene);
    EditTransformCommand cmd(e, EditTransformCommand::Field::Position,
                              glm::vec3(5, 6, 7), glm::vec3(5, 6, 7));
    CHECK(cmd.isNoOp());
}

TEST_CASE("EditTransformCommand: entidad destruida -> undo es no-op silencioso") {
    Scene scene;
    Entity e = setupEntity(scene);
    EditTransformCommand cmd(e, EditTransformCommand::Field::Position,
                              glm::vec3(0), glm::vec3(5));
    cmd.execute();
    // Destruimos la entidad — el comando guarda el handle viejo.
    scene.destroyEntity(e);
    // No debe crashear (el contrato es no-op silencioso si la entidad
    // ya no existe).
    cmd.undo();
    CHECK(true);  // si llegamos aca sin crash, OK.
}

TEST_CASE("EditTransformCommand via HistoryStack: ciclo undo/redo") {
    Scene scene;
    Entity e = setupEntity(scene, glm::vec3(0));
    HistoryStack h;
    h.push(std::make_unique<EditTransformCommand>(
        e, EditTransformCommand::Field::Position,
        glm::vec3(0), glm::vec3(10)));
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));

    REQUIRE(h.undo());
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(0.0f));

    REQUIRE(h.redo());
    CHECK(e.getComponent<TransformComponent>().position.x == doctest::Approx(10.0f));
}

TEST_CASE("EditTransformCommand: name() incluye tag de la entidad") {
    Scene scene;
    Entity e = scene.createEntity("MiCubo");
    EditTransformCommand cmd(e, EditTransformCommand::Field::Position,
                              glm::vec3(0), glm::vec3(1));
    const auto n = cmd.name();
    CHECK(n.find("Mover") != std::string::npos);
    CHECK(n.find("MiCubo") != std::string::npos);
}
