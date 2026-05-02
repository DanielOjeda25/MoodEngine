// Tests del SaveLoad (Hito 38 A): round-trip del `.moodsave` JSON.

#include <doctest/doctest.h>

#include "engine/physics/PhysicsWorld.h"
#include "engine/saving/SaveLoad.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using namespace Mood;

namespace {

std::filesystem::path tempSavePath(const std::string& tag) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("mood_save_" + tag + "_" + std::to_string(stamp) + ".moodsave");
}

} // namespace

TEST_CASE("SaveLoad: round-trip preserva campos basicos") {
    SaveLoad::SaveData d;
    d.mapPath        = "maps/level1.moodmap";
    d.hud.hp         = 75;
    d.hud.ammo       = 22;
    d.playerPosition = glm::vec3(3.5f, 1.6f, -7.25f);
    d.playerYaw      = 45.0f;
    d.playerPitch    = -10.5f;

    const auto path = tempSavePath("roundtrip");
    REQUIRE(SaveLoad::save(d, path));

    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->mapPath == "maps/level1.moodmap");
    CHECK(loaded->hud.hp == 75);
    CHECK(loaded->hud.ammo == 22);
    CHECK(loaded->playerPosition.x == doctest::Approx(3.5f));
    CHECK(loaded->playerPosition.y == doctest::Approx(1.6f));
    CHECK(loaded->playerPosition.z == doctest::Approx(-7.25f));
    CHECK(loaded->playerYaw   == doctest::Approx(45.0f));
    CHECK(loaded->playerPitch == doctest::Approx(-10.5f));

    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: load de archivo inexistente devuelve nullopt") {
    const auto fake = std::filesystem::temp_directory_path() / "no_existe_save_123.moodsave";
    std::filesystem::remove(fake);
    const auto r = SaveLoad::load(fake);
    CHECK_FALSE(r.has_value());
}

TEST_CASE("SaveLoad: load de JSON malformado devuelve nullopt") {
    const auto path = tempSavePath("malformed");
    {
        std::ofstream f(path);
        f << "esto no es JSON @#$";
    }
    const auto r = SaveLoad::load(path);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: load de version futura devuelve nullopt") {
    const auto path = tempSavePath("future_version");
    {
        std::ofstream f(path);
        f << R"({"version": 99, "map_path": "maps/future.moodmap"})";
    }
    const auto r = SaveLoad::load(path);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: round-trip con body snapshots (Hito 41 A)") {
    SaveLoad::SaveData d;
    d.mapPath = "maps/level1.moodmap";
    SaveLoad::BodySnapshot b1;
    b1.entityTag       = "Caja_01";
    b1.position        = glm::vec3(2.5f, 1.0f, -3.0f);
    b1.rotationQuat    = glm::vec4(0.0f, 0.7071f, 0.0f, 0.7071f);  // 90 Y
    b1.linearVelocity  = glm::vec3(1.0f, 0.0f, 0.5f);
    b1.angularVelocity = glm::vec3(0.0f, 1.5f, 0.0f);
    SaveLoad::BodySnapshot b2;
    b2.entityTag       = "Barril_07";
    b2.position        = glm::vec3(0.0f);
    b2.rotationQuat    = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    d.bodies.push_back(b1);
    d.bodies.push_back(b2);

    const auto path = tempSavePath("bodies");
    REQUIRE(SaveLoad::save(d, path));
    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->bodies.size() == 2);

    // Buscar por tag (orden no garantizado al leer).
    const SaveLoad::BodySnapshot* sb1 = nullptr;
    const SaveLoad::BodySnapshot* sb2 = nullptr;
    for (const auto& b : loaded->bodies) {
        if (b.entityTag == "Caja_01")    sb1 = &b;
        if (b.entityTag == "Barril_07") sb2 = &b;
    }
    REQUIRE(sb1 != nullptr);
    REQUIRE(sb2 != nullptr);
    CHECK(sb1->position.x == doctest::Approx(2.5f));
    CHECK(sb1->position.z == doctest::Approx(-3.0f));
    CHECK(sb1->rotationQuat.y == doctest::Approx(0.7071f));
    CHECK(sb1->linearVelocity.x == doctest::Approx(1.0f));
    CHECK(sb1->angularVelocity.y == doctest::Approx(1.5f));

    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: round-trip con script globals filtradas (Hito 41 B)") {
    SaveLoad::SaveData d;
    d.mapPath = "maps/level1.moodmap";
    SaveLoad::ScriptGlobalsSnapshot sg;
    sg.scriptPath = "assets/scripts/enemy.lua";
    sg.globals["hp"]        = ExposedValue{f32(75.0f)};
    sg.globals["alive"]     = ExposedValue{true};
    sg.globals["name"]      = ExposedValue{std::string("Boss")};
    sg.globals["spawn_pos"] = ExposedValue{glm::vec3(10.0f, 0.0f, -5.0f)};
    d.scriptGlobals.push_back(sg);

    const auto path = tempSavePath("globals");
    REQUIRE(SaveLoad::save(d, path));
    const auto loaded = SaveLoad::load(path);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->scriptGlobals.size() == 1);
    const auto& got = loaded->scriptGlobals[0];
    CHECK(got.scriptPath == "assets/scripts/enemy.lua");
    REQUIRE(got.globals.count("hp") == 1);
    REQUIRE(got.globals.count("alive") == 1);
    REQUIRE(got.globals.count("name") == 1);
    REQUIRE(got.globals.count("spawn_pos") == 1);
    CHECK(std::get<f32>(got.globals.at("hp")) == doctest::Approx(75.0f));
    CHECK(std::get<bool>(got.globals.at("alive")) == true);
    CHECK(std::get<std::string>(got.globals.at("name")) == "Boss");
    CHECK(std::get<glm::vec3>(got.globals.at("spawn_pos")).x == doctest::Approx(10.0f));

    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: load v1 file con campos v2 vacios (back-compat Hito 41)") {
    // V1 file = sin bodies ni script_globals. Debe cargar OK.
    const auto path = tempSavePath("v1_compat");
    {
        std::ofstream f(path);
        f << R"({
            "version": 1,
            "map_path": "maps/legacy.moodmap",
            "hud": {"hp": 50, "ammo": 10}
        })";
    }
    const auto r = SaveLoad::load(path);
    REQUIRE(r.has_value());
    CHECK(r->mapPath == "maps/legacy.moodmap");
    CHECK(r->hud.hp == 50);
    CHECK(r->bodies.empty());
    CHECK(r->scriptGlobals.empty());
    std::filesystem::remove(path);
}

TEST_CASE("SaveLoad: campos faltantes caen a defaults sin lanzar") {
    // JSON valido pero solo con version + map_path; el resto debe caer a
    // defaults del SaveData (hp=100, ammo=30, position=0, yaw=-90, pitch=0).
    const auto path = tempSavePath("partial");
    {
        std::ofstream f(path);
        f << R"({"version": 1, "map_path": "maps/partial.moodmap"})";
    }
    const auto r = SaveLoad::load(path);
    REQUIRE(r.has_value());
    CHECK(r->mapPath == "maps/partial.moodmap");
    CHECK(r->hud.hp == 100);     // default de HudState
    CHECK(r->hud.ammo == 30);    // default de HudState
    CHECK(r->playerPosition == glm::vec3(0.0f));
    CHECK(r->playerYaw == doctest::Approx(-90.0f));
    CHECK(r->playerPitch == doctest::Approx(0.0f));
    std::filesystem::remove(path);
}

// --- Hito 41 fix-up #2: createBody con rotation quat ---
// Bug raiz del Load Game: el body materializado post-load arrancaba con
// quat identidad aunque el Transform ya tuviera la rotation correcta.
// Estos tests verifican que createBody acepta quat y que el body lo
// preserva (lo lee de vuelta via bodyPositionRot).

TEST_CASE("PhysicsWorld::createBody con rotation quat preserva la pose en el body") {
    PhysicsWorld pw;

    // Quat de 45° sobre Y (yaw). w = cos(22.5°), y = sin(22.5°).
    const glm::quat qExpected = glm::angleAxis(
        glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::vec4 quatXYZW(qExpected.x, qExpected.y, qExpected.z, qExpected.w);

    const u32 id = pw.createBody(
        glm::vec3(1.0f, 2.0f, 3.0f),
        CollisionShape::Box,
        glm::vec3(0.5f),
        BodyType::Dynamic,
        /*mass*/ 1.0f, /*friction*/ 0.5f,
        quatXYZW);
    REQUIRE(id != 0);

    glm::vec4 outQuat;
    const glm::vec3 pos = pw.bodyPositionRot(id, outQuat);
    CHECK(pos.x == doctest::Approx(1.0f));
    CHECK(pos.y == doctest::Approx(2.0f));
    CHECK(pos.z == doctest::Approx(3.0f));
    // Comparamos componentes del quat. Los signos pueden cambiar
    // (q y -q representan la misma rotacion); chequeamos abs.
    CHECK(std::abs(outQuat.x) == doctest::Approx(std::abs(quatXYZW.x)).epsilon(0.001));
    CHECK(std::abs(outQuat.y) == doctest::Approx(std::abs(quatXYZW.y)).epsilon(0.001));
    CHECK(std::abs(outQuat.z) == doctest::Approx(std::abs(quatXYZW.z)).epsilon(0.001));
    CHECK(std::abs(outQuat.w) == doctest::Approx(std::abs(quatXYZW.w)).epsilon(0.001));
}

TEST_CASE("PhysicsWorld::createBody sin rotation explicita usa identidad") {
    // Compatibilidad con call sites pre-fix que no pasan el parametro.
    PhysicsWorld pw;
    const u32 id = pw.createBody(
        glm::vec3(0.0f), CollisionShape::Box, glm::vec3(0.5f),
        BodyType::Static);
    REQUIRE(id != 0);

    glm::vec4 outQuat;
    pw.bodyPositionRot(id, outQuat);
    // Identidad: (0, 0, 0, 1) con tolerancia.
    CHECK(outQuat.x == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(outQuat.y == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(outQuat.z == doctest::Approx(0.0f).epsilon(0.001));
    CHECK(std::abs(outQuat.w) == doctest::Approx(1.0f).epsilon(0.001));
}

// --- Hito 41 fix-up #2: pending velocidades del SaveLoad ---
// Cuando applyLoadedSave corre antes de que el body se materialice
// (caso "Load Game directo desde el Main Menu"), las vels del snapshot
// se stash en `RigidBodyComponent::pendingLinearVel/AngularVel`.
// Tras `updateRigidBodies` materializar el body, las pending se aplican
// y el flag se limpia. Aca testeamos esa pieza pura: armar el rb con
// pending, llamar createBody + setVel + clear, leer del body.

TEST_CASE("RigidBodyComponent: pending vels aplicadas al materializar") {
    PhysicsWorld pw;

    // Simulamos lo que applyLoadedSave deja: el snapshot del save tenia
    // linearVel=(2,0,0) y angularVel=(0,1,0); el body aun no existe.
    RigidBodyComponent rb{
        RigidBodyComponent::Type::Dynamic,
        RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f), 5.0f};
    rb.hasPendingVel = true;
    rb.pendingLinearVel  = glm::vec3(2.0f, 0.0f, 0.0f);
    rb.pendingAngularVel = glm::vec3(0.0f, 1.0f, 0.0f);

    // updateRigidBodies-like: crea el body + aplica pending + limpia.
    rb.bodyId = pw.createBody(glm::vec3(0.0f, 5.0f, 0.0f),
                                CollisionShape::Box, rb.halfExtents,
                                BodyType::Dynamic, rb.mass, rb.friction);
    REQUIRE(rb.bodyId != 0);
    if (rb.hasPendingVel) {
        pw.setBodyLinearVelocity(rb.bodyId, rb.pendingLinearVel);
        pw.setBodyAngularVelocity(rb.bodyId, rb.pendingAngularVel);
        rb.hasPendingVel = false;
        rb.pendingLinearVel = glm::vec3(0.0f);
        rb.pendingAngularVel = glm::vec3(0.0f);
    }

    CHECK_FALSE(rb.hasPendingVel);
    const glm::vec3 v = pw.bodyLinearVelocity(rb.bodyId);
    const glm::vec3 w = pw.bodyAngularVelocity(rb.bodyId);
    CHECK(v.x == doctest::Approx(2.0f).epsilon(0.01));
    CHECK(w.y == doctest::Approx(1.0f).epsilon(0.01));
}

TEST_CASE("RigidBodyComponent: hasPendingVel=false → no se aplica nada al materializar") {
    PhysicsWorld pw;
    RigidBodyComponent rb{
        RigidBodyComponent::Type::Dynamic,
        RigidBodyComponent::Shape::Box,
        glm::vec3(0.5f), 1.0f};
    // Sin pending — caso normal de spawn fresco sin Load.
    REQUIRE_FALSE(rb.hasPendingVel);

    rb.bodyId = pw.createBody(glm::vec3(0.0f), CollisionShape::Box,
                                rb.halfExtents, BodyType::Dynamic,
                                rb.mass, rb.friction);
    REQUIRE(rb.bodyId != 0);

    const glm::vec3 v = pw.bodyLinearVelocity(rb.bodyId);
    CHECK(v.x == doctest::Approx(0.0f));
    CHECK(v.y == doctest::Approx(0.0f));
    CHECK(v.z == doctest::Approx(0.0f));
}
