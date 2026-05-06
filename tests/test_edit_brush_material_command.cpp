// Tests del EditBrushMaterialCommand (F2H16). Round-trip:
//   - execute aplica newMat al brush.
//   - undo restaura oldMat.
//   - redo (execute tras undo) re-aplica newMat.
//   - Tag invalido: el comando hace warn pero no crash.
//   - Brush sin BrushComponent: warn, no crash.

#include <doctest/doctest.h>

#include "editor/commands/EditBrushMaterialCommand.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"

using namespace Mood;

namespace {

Entity makeBrushEntity(Scene& scene, const std::string& tag,
                        MaterialAssetId mat) {
    Entity e = scene.createEntity(tag);
    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    bc.materials = {mat};
    e.addComponent<BrushComponent>(std::move(bc));
    return e;
}

}  // namespace

TEST_CASE("EditBrushMaterialCommand: execute aplica newMat") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", 5);

    EditBrushMaterialCommand cmd(&scene, "Brush1", 5, 7,
                                   "Asignar material a brush");
    cmd.execute();

    CHECK(e.getComponent<BrushComponent>().materials[0] == 7);
    CHECK(e.getComponent<BrushComponent>().dirty);
}

TEST_CASE("EditBrushMaterialCommand: undo restaura oldMat") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", 5);

    EditBrushMaterialCommand cmd(&scene, "Brush1", 5, 7);
    cmd.execute();
    REQUIRE(e.getComponent<BrushComponent>().materials[0] == 7);

    cmd.undo();
    CHECK(e.getComponent<BrushComponent>().materials[0] == 5);
}

TEST_CASE("EditBrushMaterialCommand: redo (execute tras undo)") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", 0);

    EditBrushMaterialCommand cmd(&scene, "Brush1", 0, 42);
    cmd.execute();
    cmd.undo();
    cmd.execute();
    CHECK(e.getComponent<BrushComponent>().materials[0] == 42);
}

TEST_CASE("EditBrushMaterialCommand: tag inexistente no crashea") {
    Scene scene;
    EditBrushMaterialCommand cmd(&scene, "NoExiste", 0, 5);
    cmd.execute();  // warn, no crash
    cmd.undo();      // warn, no crash
    CHECK(true);
}

TEST_CASE("EditBrushMaterialCommand: name() devuelve label") {
    Scene scene;
    EditBrushMaterialCommand cmd(&scene, "Brush1", 0, 5,
                                   "Mi label custom");
    CHECK(cmd.name() == "Mi label custom");
}
