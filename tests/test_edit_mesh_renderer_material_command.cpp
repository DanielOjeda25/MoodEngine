// Tests del EditMeshRendererMaterialCommand (F2H16). Round-trip
// del slot 0 del MeshRendererComponent.

#include <doctest/doctest.h>

#include "editor/commands/EditMeshRendererMaterialCommand.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

using namespace Mood;

TEST_CASE("EditMeshRendererMaterialCommand: execute setea slot 0") {
    Scene scene;
    Entity e = scene.createEntity("MeshA");
    e.addComponent<MeshRendererComponent>(0u, 5u);  // mesh=0, materials=[5]

    EditMeshRendererMaterialCommand cmd(&scene, "MeshA", 0, 5, 9);
    cmd.execute();
    CHECK(e.getComponent<MeshRendererComponent>().materials[0] == 9);
}

TEST_CASE("EditMeshRendererMaterialCommand: undo restaura oldMat") {
    Scene scene;
    Entity e = scene.createEntity("MeshA");
    e.addComponent<MeshRendererComponent>(0u, 5u);

    EditMeshRendererMaterialCommand cmd(&scene, "MeshA", 0, 5, 9);
    cmd.execute();
    cmd.undo();
    CHECK(e.getComponent<MeshRendererComponent>().materials[0] == 5);
}

TEST_CASE("EditMeshRendererMaterialCommand: slot vacio se crea con resize") {
    Scene scene;
    Entity e = scene.createEntity("MeshEmpty");
    auto& mr = e.addComponent<MeshRendererComponent>();
    mr.materials.clear();  // slot 0 no existe

    EditMeshRendererMaterialCommand cmd(&scene, "MeshEmpty", 0, 0, 7);
    cmd.execute();
    REQUIRE(e.getComponent<MeshRendererComponent>().materials.size() >= 1);
    CHECK(e.getComponent<MeshRendererComponent>().materials[0] == 7);
}

TEST_CASE("EditMeshRendererMaterialCommand: name") {
    Scene scene;
    EditMeshRendererMaterialCommand cmd(&scene, "X", 0, 0, 1, "Mat label");
    CHECK(cmd.name() == "Mat label");
}
