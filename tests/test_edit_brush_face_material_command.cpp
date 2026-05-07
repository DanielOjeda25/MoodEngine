// Tests del EditBrushFaceMaterialCommand (F2H19). Round-trip:
//   - execute aplica newMaterials + newFaceMatIndex.
//   - undo restaura oldMaterials + oldFaceMatIndex.
//   - redo idempotente.
//   - Caso "agregar slot nuevo": oldMaterials.size()=1, newMaterials.size()=2.
//     Undo debe quedar con tamano 1 (no slot huerfano).
//   - Tag invalido: warn, no crash.
//   - Brush sin BrushComponent: warn, no crash.
//   - faceIndex fuera de rango: warn, no crash.

#include <doctest/doctest.h>

#include "editor/commands/EditBrushFaceMaterialCommand.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"

#include <vector>

using namespace Mood;

namespace {

Entity makeBrushEntity(Scene& scene, const std::string& tag,
                        const std::vector<MaterialAssetId>& mats,
                        u32 faceMatIndex = 0) {
    Entity e = scene.createEntity(tag);
    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    bc.materials = mats;
    if (!bc.brush.faces.empty()) {
        bc.brush.faces[0].materialIndex = faceMatIndex;
    }
    e.addComponent<BrushComponent>(std::move(bc));
    return e;
}

}  // namespace

TEST_CASE("EditBrushFaceMaterialCommand: execute aplica nuevo slot a la cara") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);

    // Drop de un material nuevo: agrega slot 1 con id=7, face[0] -> slot 1.
    EditBrushFaceMaterialCommand cmd(&scene, "Brush1", /*faceIndex=*/0,
                                       /*oldMaterials=*/{5}, /*oldFaceMatIndex=*/0,
                                       /*newMaterials=*/{5, 7}, /*newFaceMatIndex=*/1,
                                       "Asignar textura a cara");
    cmd.execute();

    auto& bc = e.getComponent<BrushComponent>();
    REQUIRE(bc.materials.size() == 2u);
    CHECK(bc.materials[0] == 5);
    CHECK(bc.materials[1] == 7);
    CHECK(bc.brush.faces[0].materialIndex == 1u);
    CHECK(bc.dirty);
}

TEST_CASE("EditBrushFaceMaterialCommand: undo restaura materials y faceMatIndex") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);

    EditBrushFaceMaterialCommand cmd(&scene, "Brush1", 0,
                                       {5}, 0,
                                       {5, 7}, 1);
    cmd.execute();
    REQUIRE(e.getComponent<BrushComponent>().materials.size() == 2u);

    cmd.undo();
    auto& bc = e.getComponent<BrushComponent>();
    // Critico: sin slot huerfano post-undo.
    REQUIRE(bc.materials.size() == 1u);
    CHECK(bc.materials[0] == 5);
    CHECK(bc.brush.faces[0].materialIndex == 0u);
}

TEST_CASE("EditBrushFaceMaterialCommand: redo (execute tras undo) reaplica") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);

    EditBrushFaceMaterialCommand cmd(&scene, "Brush1", 0,
                                       {5}, 0,
                                       {5, 7}, 1);
    cmd.execute();
    cmd.undo();
    cmd.execute();

    auto& bc = e.getComponent<BrushComponent>();
    REQUIRE(bc.materials.size() == 2u);
    CHECK(bc.materials[1] == 7);
    CHECK(bc.brush.faces[0].materialIndex == 1u);
}

TEST_CASE("EditBrushFaceMaterialCommand: reusar slot existente (sin agregar) preserva tamano") {
    Scene scene;
    // brush ya tiene 2 slots; el drop reusa el slot 1 (no push_back).
    Entity e = makeBrushEntity(scene, "Brush1", {5, 7}, 0);

    EditBrushFaceMaterialCommand cmd(&scene, "Brush1", 0,
                                       {5, 7}, 0,
                                       {5, 7}, 1);
    cmd.execute();
    auto& bc = e.getComponent<BrushComponent>();
    CHECK(bc.materials.size() == 2u);
    CHECK(bc.brush.faces[0].materialIndex == 1u);

    cmd.undo();
    CHECK(bc.materials.size() == 2u);
    CHECK(bc.brush.faces[0].materialIndex == 0u);
}

TEST_CASE("EditBrushFaceMaterialCommand: tag inexistente no crashea") {
    Scene scene;
    EditBrushFaceMaterialCommand cmd(&scene, "NoExiste", 0,
                                       {0}, 0, {0, 5}, 1);
    cmd.execute();  // warn, no crash
    cmd.undo();      // warn, no crash
    CHECK(true);
}

TEST_CASE("EditBrushFaceMaterialCommand: faceIndex fuera de rango no crashea") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);

    EditBrushFaceMaterialCommand cmd(&scene, "Brush1", /*faceIndex=*/999,
                                       {5}, 0, {5, 7}, 1);
    cmd.execute();  // warn, no crash
    // La face 0 no debe haberse mutado.
    CHECK(e.getComponent<BrushComponent>().brush.faces[0].materialIndex == 0u);
}

TEST_CASE("EditBrushFaceMaterialCommand: name() devuelve label") {
    Scene scene;
    EditBrushFaceMaterialCommand cmd(&scene, "Brush1", 0,
                                       {0}, 0, {0, 5}, 1,
                                       "Mi label custom");
    CHECK(cmd.name() == "Mi label custom");
}
