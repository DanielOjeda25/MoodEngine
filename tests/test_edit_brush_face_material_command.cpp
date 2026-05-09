// Tests del EditBrushFaceMaterialCommand (F2H19 + F2H34). Round-trip:
//   - execute aplica newMaterials + newFaceMatIndex.
//   - undo restaura oldMaterials + oldFaceMatIndex.
//   - redo idempotente.
//   - Caso "agregar slot nuevo": oldMaterials.size()=1, newMaterials.size()=2.
//     Undo debe quedar con tamano 1 (no slot huerfano).
//   - Tag invalido: warn, no crash.
//   - Brush sin BrushComponent: warn, no crash.
//   - faceIndex fuera de rango: warn, no crash.
//
// F2H34 (multi-cara):
//   - execute aplica el slot a las N caras del set.
//   - undo restaura los faceMatIndex previos POR cara (no uniforme).
//   - "agregar slot nuevo" comparte el slot entre todas las caras
//     (un solo push_back, no N).
//   - faceIndex fuera de rango en cualquier posicion -> no muta nada
//     (todo o nada).

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

// ------------------------------------------------------------------
// F2H34: tests multi-cara
// ------------------------------------------------------------------

namespace {

// Setea materialIndex en N caras del brush para tener un baseline
// heterogeneo previo a un drop multi-cara.
void setFaceMatIndices(Entity e,
                        const std::vector<u32>& faceIndices,
                        const std::vector<u32>& matIndices) {
    auto& bc = e.getComponent<BrushComponent>();
    for (size_t i = 0; i < faceIndices.size(); ++i) {
        bc.brush.faces[faceIndices[i]].materialIndex = matIndices[i];
    }
}

}  // namespace

TEST_CASE("EditBrushFaceMaterialCommand multi: aplica el mismo slot a 3 caras") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);
    // Caras 0,1,2 todas en slot 0 (material 5). Drop de un material
    // nuevo (id=7) sobre las 3.
    setFaceMatIndices(e, {0, 1, 2}, {0, 0, 0});

    EditBrushFaceMaterialCommand cmd(
        &scene, "Brush1",
        /*faceIndices=*/std::vector<u32>{0, 1, 2},
        /*oldMaterials=*/{5}, /*oldFaceMatIndices=*/{0, 0, 0},
        /*newMaterials=*/{5, 7}, /*newFaceMatIndices=*/{1, 1, 1},
        "Asignar textura a caras");
    cmd.execute();

    auto& bc = e.getComponent<BrushComponent>();
    REQUIRE(bc.materials.size() == 2u);
    CHECK(bc.materials[1] == 7);
    CHECK(bc.brush.faces[0].materialIndex == 1u);
    CHECK(bc.brush.faces[1].materialIndex == 1u);
    CHECK(bc.brush.faces[2].materialIndex == 1u);
    CHECK(bc.dirty);
}

TEST_CASE("EditBrushFaceMaterialCommand multi: undo restaura faceMatIndex heterogeneo") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5, 9}, 0);
    // Pre: caras 0,1,2 con materiales DISTINTOS — 0->slot 0, 1->slot 1,
    // 2->slot 0. Tras el drop de id=7, todas pasan al slot 2 (nuevo).
    // Undo debe restaurar el patron heterogeneo previo.
    setFaceMatIndices(e, {0, 1, 2}, {0, 1, 0});

    EditBrushFaceMaterialCommand cmd(
        &scene, "Brush1",
        std::vector<u32>{0, 1, 2},
        {5, 9}, std::vector<u32>{0, 1, 0},
        {5, 9, 7}, std::vector<u32>{2, 2, 2},
        "Asignar textura a caras");
    cmd.execute();

    auto& bc = e.getComponent<BrushComponent>();
    REQUIRE(bc.materials.size() == 3u);
    CHECK(bc.brush.faces[0].materialIndex == 2u);
    CHECK(bc.brush.faces[1].materialIndex == 2u);
    CHECK(bc.brush.faces[2].materialIndex == 2u);

    cmd.undo();
    // Critico post-F2H34: undo NO uniforme, restaura el patron exacto.
    REQUIRE(bc.materials.size() == 2u);
    CHECK(bc.brush.faces[0].materialIndex == 0u);
    CHECK(bc.brush.faces[1].materialIndex == 1u);
    CHECK(bc.brush.faces[2].materialIndex == 0u);
}

TEST_CASE("EditBrushFaceMaterialCommand multi: redo tras undo reaplica el slot a todas") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);
    setFaceMatIndices(e, {0, 1, 2}, {0, 0, 0});

    EditBrushFaceMaterialCommand cmd(
        &scene, "Brush1",
        std::vector<u32>{0, 1, 2},
        {5}, std::vector<u32>{0, 0, 0},
        {5, 7}, std::vector<u32>{1, 1, 1});
    cmd.execute();
    cmd.undo();
    cmd.execute();

    auto& bc = e.getComponent<BrushComponent>();
    REQUIRE(bc.materials.size() == 2u);
    CHECK(bc.brush.faces[0].materialIndex == 1u);
    CHECK(bc.brush.faces[1].materialIndex == 1u);
    CHECK(bc.brush.faces[2].materialIndex == 1u);
}

TEST_CASE("EditBrushFaceMaterialCommand multi: faceIndex fuera de rango en una cara -> no muta nada") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);
    setFaceMatIndices(e, {0, 1}, {0, 0});

    // 999 esta fuera de rango. La validacion es atomica: ninguna cara
    // se muta (ni siquiera la valida).
    EditBrushFaceMaterialCommand cmd(
        &scene, "Brush1",
        std::vector<u32>{0, 999, 1},
        {5}, std::vector<u32>{0, 0, 0},
        {5, 7}, std::vector<u32>{1, 1, 1});
    cmd.execute();

    auto& bc = e.getComponent<BrushComponent>();
    // Sin slot agregado, sin caras mutadas.
    CHECK(bc.materials.size() == 1u);
    CHECK(bc.brush.faces[0].materialIndex == 0u);
    CHECK(bc.brush.faces[1].materialIndex == 0u);
}

TEST_CASE("EditBrushFaceMaterialCommand multi: ctor 1-cara wrappea al multi (back-compat)") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "Brush1", {5}, 0);

    // El ctor 1-cara debe seguir funcionando exactamente igual que pre-F2H34.
    EditBrushFaceMaterialCommand cmd(&scene, "Brush1", /*faceIndex=*/0,
                                       {5}, 0,
                                       {5, 7}, 1);
    cmd.execute();
    auto& bc = e.getComponent<BrushComponent>();
    REQUIRE(bc.materials.size() == 2u);
    CHECK(bc.brush.faces[0].materialIndex == 1u);

    cmd.undo();
    REQUIRE(bc.materials.size() == 1u);
    CHECK(bc.brush.faces[0].materialIndex == 0u);
}
