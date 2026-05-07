// Tests del EditScriptComponentCommand (F2H19). Cubre los 2 sub-casos:
//   1. Entidad NO tenia ScriptComponent: execute lo agrega; undo lo remueve.
//   2. Entidad SI tenia: execute reemplaza path; undo restaura path previo.
// Mas redo idempotente, tag invalido, edge case de "alguien removio el
// componente entre execute y undo".

#include <doctest/doctest.h>

#include "editor/commands/EditScriptComponentCommand.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

using namespace Mood;

namespace {

Entity makeEntity(Scene& scene, const std::string& tag) {
    return scene.createEntity(tag);
}

Entity makeEntityWithScript(Scene& scene, const std::string& tag,
                              const std::string& path) {
    Entity e = scene.createEntity(tag);
    e.addComponent<ScriptComponent>(path);
    return e;
}

}  // namespace

TEST_CASE("EditScriptComponentCommand: execute en entidad SIN componente lo agrega") {
    Scene scene;
    Entity e = makeEntity(scene, "Ent1");
    REQUIRE_FALSE(e.hasComponent<ScriptComponent>());

    EditScriptComponentCommand cmd(&scene, "Ent1",
                                     /*hadComponent=*/false,
                                     /*oldPath=*/"",
                                     /*newPath=*/"assets/scripts/foo.lua",
                                     "Asignar script a entidad");
    cmd.execute();

    REQUIRE(e.hasComponent<ScriptComponent>());
    CHECK(e.getComponent<ScriptComponent>().path == "assets/scripts/foo.lua");
    CHECK_FALSE(e.getComponent<ScriptComponent>().loaded);
}

TEST_CASE("EditScriptComponentCommand: undo en entidad SIN componente lo remueve") {
    Scene scene;
    Entity e = makeEntity(scene, "Ent1");

    EditScriptComponentCommand cmd(&scene, "Ent1", false, "",
                                     "assets/scripts/foo.lua");
    cmd.execute();
    REQUIRE(e.hasComponent<ScriptComponent>());

    cmd.undo();
    CHECK_FALSE(e.hasComponent<ScriptComponent>());
}

TEST_CASE("EditScriptComponentCommand: execute en entidad CON componente reemplaza path") {
    Scene scene;
    Entity e = makeEntityWithScript(scene, "Ent1", "assets/scripts/old.lua");

    EditScriptComponentCommand cmd(&scene, "Ent1",
                                     /*hadComponent=*/true,
                                     /*oldPath=*/"assets/scripts/old.lua",
                                     /*newPath=*/"assets/scripts/new.lua");
    cmd.execute();

    REQUIRE(e.hasComponent<ScriptComponent>());
    CHECK(e.getComponent<ScriptComponent>().path == "assets/scripts/new.lua");
}

TEST_CASE("EditScriptComponentCommand: undo en entidad CON componente restaura path previo") {
    Scene scene;
    Entity e = makeEntityWithScript(scene, "Ent1", "assets/scripts/old.lua");

    EditScriptComponentCommand cmd(&scene, "Ent1", true,
                                     "assets/scripts/old.lua",
                                     "assets/scripts/new.lua");
    cmd.execute();
    REQUIRE(e.getComponent<ScriptComponent>().path == "assets/scripts/new.lua");

    cmd.undo();
    REQUIRE(e.hasComponent<ScriptComponent>());
    CHECK(e.getComponent<ScriptComponent>().path == "assets/scripts/old.lua");
}

TEST_CASE("EditScriptComponentCommand: redo idempotente") {
    Scene scene;
    Entity e = makeEntity(scene, "Ent1");

    EditScriptComponentCommand cmd(&scene, "Ent1", false, "",
                                     "assets/scripts/foo.lua");
    cmd.execute();
    cmd.undo();
    cmd.execute();

    REQUIRE(e.hasComponent<ScriptComponent>());
    CHECK(e.getComponent<ScriptComponent>().path == "assets/scripts/foo.lua");
}

TEST_CASE("EditScriptComponentCommand: tag inexistente no crashea") {
    Scene scene;
    EditScriptComponentCommand cmd(&scene, "NoExiste", false, "",
                                     "assets/scripts/foo.lua");
    cmd.execute();
    cmd.undo();
    CHECK(true);
}

TEST_CASE("EditScriptComponentCommand: undo cuando alguien removio el componente recrea con oldPath") {
    Scene scene;
    Entity e = makeEntityWithScript(scene, "Ent1", "assets/scripts/old.lua");

    EditScriptComponentCommand cmd(&scene, "Ent1", true,
                                     "assets/scripts/old.lua",
                                     "assets/scripts/new.lua");
    cmd.execute();
    // Simulamos que un agente externo borra el componente.
    e.removeComponent<ScriptComponent>();
    REQUIRE_FALSE(e.hasComponent<ScriptComponent>());

    cmd.undo();
    REQUIRE(e.hasComponent<ScriptComponent>());
    CHECK(e.getComponent<ScriptComponent>().path == "assets/scripts/old.lua");
}

TEST_CASE("EditScriptComponentCommand: name() devuelve label") {
    Scene scene;
    EditScriptComponentCommand cmd(&scene, "Ent1", false, "",
                                     "assets/scripts/foo.lua",
                                     "Mi label custom");
    CHECK(cmd.name() == "Mi label custom");
}
