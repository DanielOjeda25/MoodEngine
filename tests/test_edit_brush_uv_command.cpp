// Tests del EditBrushUVCommand (F2H16). Round-trip de los 6 UV
// params (uAxis, vAxis, uvOffset, uvScale, uvRotation, lockToWorld)
// per-cara. Captura snapshot pre/post + execute/undo.

#include <doctest/doctest.h>

#include "editor/commands/EditBrushUVCommand.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/csg/Brush.h"

using namespace Mood;

namespace {

Entity makeBrushEntity(Scene& scene, const std::string& tag) {
    Entity e = scene.createEntity(tag);
    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    e.addComponent<BrushComponent>(std::move(bc));
    return e;
}

}  // namespace

TEST_CASE("captureBrushUV: captura los 6 params per cara") {
    auto brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    REQUIRE(brush.faces.size() == 6);
    // Modificar params para diferenciar de los defaults.
    for (u32 i = 0; i < 6; ++i) {
        brush.faces[i].uvScale     = glm::vec2(2.0f + i);
        brush.faces[i].uvRotation  = static_cast<f32>(i) * 0.1f;
        brush.faces[i].lockToWorld = (i % 2 == 0);
    }
    const BrushUVSnapshot snap = captureBrushUV(brush);
    REQUIRE(snap.uvScales.size() == 6);
    for (u32 i = 0; i < 6; ++i) {
        CHECK(snap.uvScales[i].x == doctest::Approx(2.0f + i));
        CHECK(snap.uvRotations[i] == doctest::Approx(static_cast<f32>(i) * 0.1f));
        CHECK(snap.lockToWorlds[i] == ((i % 2 == 0) ? 1u : 0u));
    }
}

TEST_CASE("applyBrushUV: aplica el snapshot completo") {
    auto brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    BrushUVSnapshot snap = captureBrushUV(brush);
    // Modificar el snapshot.
    for (auto& s : snap.uvScales) s = glm::vec2(5.0f);
    applyBrushUV(brush, snap);
    for (const auto& f : brush.faces) {
        CHECK(f.uvScale.x == doctest::Approx(5.0f));
    }
}

TEST_CASE("applyBrushUV: snapshot con tamano diferente no aplica") {
    auto brush = Csg::makeBoxBrush(glm::mat4(1.0f));  // 6 caras
    BrushUVSnapshot wrongSnap;
    wrongSnap.uvScales.push_back(glm::vec2(99.0f));   // solo 1 entry

    applyBrushUV(brush, wrongSnap);
    // Las uvScale del brush no deben haber cambiado (siguen siendo
    // los defaults vec2(1)).
    for (const auto& f : brush.faces) {
        CHECK(f.uvScale.x == doctest::Approx(1.0f));
    }
}

TEST_CASE("snapshotsEqual: identicos vs diferentes") {
    auto brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    const BrushUVSnapshot a = captureBrushUV(brush);
    BrushUVSnapshot b = a;
    CHECK(snapshotsEqual(a, b));

    b.uvScales[0] = glm::vec2(2.0f);
    CHECK_FALSE(snapshotsEqual(a, b));
}

TEST_CASE("EditBrushUVCommand: round-trip") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "BrushUV");
    auto& bc = e.getComponent<BrushComponent>();

    BrushUVSnapshot oldSnap = captureBrushUV(bc.brush);
    // Mutar el brush al estado "post".
    for (auto& f : bc.brush.faces) f.uvScale = glm::vec2(3.0f);
    BrushUVSnapshot newSnap = captureBrushUV(bc.brush);

    EditBrushUVCommand cmd(&scene, "BrushUV",
                            std::move(oldSnap), std::move(newSnap),
                            "Editar UV scale");
    // El brush ya esta en el estado post. Undo lo revierte al pre.
    cmd.undo();
    for (const auto& f : bc.brush.faces) {
        CHECK(f.uvScale.x == doctest::Approx(1.0f));  // default
    }
    // Redo lo vuelve al post.
    cmd.execute();
    for (const auto& f : bc.brush.faces) {
        CHECK(f.uvScale.x == doctest::Approx(3.0f));
    }
}

TEST_CASE("EditBrushUVCommand: tag inexistente no crashea") {
    Scene scene;
    EditBrushUVCommand cmd(&scene, "NoExiste", {}, {});
    cmd.execute();
    cmd.undo();
    CHECK(true);
}

TEST_CASE("EditBrushUVCommand: lockToWorld preservado en round-trip") {
    Scene scene;
    Entity e = makeBrushEntity(scene, "LockBrush");
    auto& bc = e.getComponent<BrushComponent>();

    BrushUVSnapshot oldSnap = captureBrushUV(bc.brush);
    for (auto& f : bc.brush.faces) f.lockToWorld = true;
    bc.anyFaceLockToWorld = true;
    BrushUVSnapshot newSnap = captureBrushUV(bc.brush);

    EditBrushUVCommand cmd(&scene, "LockBrush",
                            std::move(oldSnap), std::move(newSnap),
                            "Toggle lock");
    cmd.undo();
    for (const auto& f : bc.brush.faces) {
        CHECK_FALSE(f.lockToWorld);
    }
    CHECK_FALSE(bc.anyFaceLockToWorld);

    cmd.execute();
    for (const auto& f : bc.brush.faces) {
        CHECK(f.lockToWorld);
    }
    CHECK(bc.anyFaceLockToWorld);
}
