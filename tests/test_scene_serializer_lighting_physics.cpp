// Tests de SceneSerializer — split AUDIT-3 (familia: lighting + physics).
// Roundtrip de LightComponent (Hito 11, 16) + RigidBodyComponent
// (Hito 12, 34 A). Comparte helpers con test_scene_serializer.cpp via
// test_scene_serializer_helpers.h.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneLoader.h"  // F2H65
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/grid/GridMap.h"
#include "test_scene_serializer_helpers.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>

using namespace Mood;
using Mood::SceneSerializerTests::nullFactory;
using Mood::SceneSerializerTests::tempPath;

TEST_CASE("SceneSerializer: round-trip de LightComponent (Hito 11)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity p = scene.createEntity("Luz_demo");
    {
        auto& t = p.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 4.0f, 0.0f);
        LightComponent lc{};
        lc.type      = LightComponent::Type::Point;
        lc.color     = glm::vec3(1.0f, 0.95f, 0.85f);
        lc.intensity = 1.5f;
        lc.radius    = 12.0f;
        lc.enabled   = true;
        p.addComponent<LightComponent>(lc);
    }
    Entity d = scene.createEntity("Sol");
    {
        LightComponent lc{};
        lc.type        = LightComponent::Type::Directional;
        lc.direction   = glm::vec3(0.0f, -1.0f, 0.3f);
        lc.color       = glm::vec3(1.0f, 0.8f, 0.6f);
        lc.intensity   = 0.5f;
        lc.enabled     = true;
        lc.castShadows = true; // Hito 16
        d.addComponent<LightComponent>(lc);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("light_roundtrip.moodmap");
    SceneSerializer::save(empty, "lit", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 2);

    const SavedEntity* sePoint = nullptr;
    const SavedEntity* seDir   = nullptr;
    for (const auto& se : loaded->entities) {
        if (se.tag == "Luz_demo") sePoint = &se;
        if (se.tag == "Sol")      seDir   = &se;
    }
    REQUIRE(sePoint != nullptr);
    REQUIRE(seDir   != nullptr);
    REQUIRE(sePoint->light.has_value());
    REQUIRE(seDir->light.has_value());

    CHECK(sePoint->light->type == "point");
    CHECK(sePoint->light->intensity == doctest::Approx(1.5f));
    CHECK(sePoint->light->radius    == doctest::Approx(12.0f));
    CHECK(sePoint->light->color.r   == doctest::Approx(1.0f));
    CHECK(sePoint->light->color.g   == doctest::Approx(0.95f));
    CHECK(sePoint->light->enabled);
    CHECK_FALSE(sePoint->light->castShadows);

    CHECK(seDir->light->type == "directional");
    CHECK(seDir->light->direction.y == doctest::Approx(-1.0f));
    CHECK(seDir->light->intensity   == doctest::Approx(0.5f));
    CHECK(seDir->light->castShadows);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de RigidBodyComponent (Hito 12)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity box = scene.createEntity("CajaFisica");
    {
        auto& t = box.getComponent<TransformComponent>();
        t.position = glm::vec3(2.5f, 6.0f, -1.5f);
        t.scale = glm::vec3(1.0f);
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(0.5f, 0.75f, 0.5f);
        rb.mass = 5.0f;
        box.addComponent<RigidBodyComponent>(rb);
    }

    Entity floor = scene.createEntity("Piso");
    {
        auto& t = floor.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f);
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        rb.shape = RigidBodyComponent::Shape::Sphere;
        rb.halfExtents = glm::vec3(2.0f);
        floor.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("rigidbody_roundtrip.moodmap");
    SceneSerializer::save(empty, "physics", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 2);

    const SavedEntity* seBox = nullptr;
    const SavedEntity* seFloor = nullptr;
    for (const auto& se : loaded->entities) {
        if (se.tag == "CajaFisica") seBox = &se;
        if (se.tag == "Piso")       seFloor = &se;
    }
    REQUIRE(seBox   != nullptr);
    REQUIRE(seFloor != nullptr);
    REQUIRE(seBox->rigidBody.has_value());
    REQUIRE(seFloor->rigidBody.has_value());

    CHECK(seBox->rigidBody->type  == "dynamic");
    CHECK(seBox->rigidBody->shape == "box");
    CHECK(seBox->rigidBody->mass  == doctest::Approx(5.0f));
    CHECK(seBox->rigidBody->halfExtents.x == doctest::Approx(0.5f));
    CHECK(seBox->rigidBody->halfExtents.y == doctest::Approx(0.75f));

    CHECK(seFloor->rigidBody->type  == "static");
    CHECK(seFloor->rigidBody->shape == "sphere");
    CHECK(seFloor->rigidBody->halfExtents.x == doctest::Approx(2.0f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: friction custom value round-trip (Hito 34 A)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity ice = scene.createEntity("Hielo");
    {
        RigidBodyComponent rb{};
        rb.type        = RigidBodyComponent::Type::Static;
        rb.shape       = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(2.0f, 0.1f, 2.0f);
        rb.friction    = 0.05f;
        ice.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("friction_custom.moodmap");
    SceneSerializer::save(empty, "ice", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);
    REQUIRE(loaded->entities[0].rigidBody.has_value());
    CHECK(loaded->entities[0].rigidBody->friction == doctest::Approx(0.05f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip JointComponent (F2H65 Hinge con limits)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    // Setup: A = puerta (dynamic), B = marco (static). A tiene Joint Hinge
    // limitado a [0, 90] grados con pivot al borde y axis Y vertical.
    Entity frame = scene.createEntity("Marco");
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(0.1f, 1.0f, 0.05f);
        frame.addComponent<RigidBodyComponent>(rb);
    }
    Entity door = scene.createEntity("Puerta");
    {
        auto& t = door.getComponent<TransformComponent>();
        t.position = glm::vec3(0.5f, 1.0f, 0.0f);
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(0.5f, 1.0f, 0.05f);
        rb.mass = 5.0f;
        door.addComponent<RigidBodyComponent>(rb);
        JointComponent jc{};
        jc.type         = JointComponent::Type::Hinge;
        jc.targetEntity = static_cast<u32>(frame.handle());
        jc.pivotLocal   = glm::vec3(-0.5f, 0.0f, 0.0f);  // borde izquierdo
        jc.axisLocal    = glm::vec3(0.0f, 1.0f, 0.0f);   // vertical
        jc.limitMinDeg  = 0.0f;
        jc.limitMaxDeg  = 90.0f;
        door.addComponent<JointComponent>(jc);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("joint_hinge_roundtrip.moodmap");
    SceneSerializer::save(empty, "door", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const SavedEntity* seDoor = nullptr;
    for (const auto& se : loaded->entities) {
        if (se.tag == "Puerta") seDoor = &se;
    }
    REQUIRE(seDoor != nullptr);
    REQUIRE(seDoor->joint.has_value());
    CHECK(seDoor->joint->type       == "hinge");
    CHECK(seDoor->joint->targetTag  == "Marco");
    CHECK(seDoor->joint->pivotLocal.x  == doctest::Approx(-0.5f));
    CHECK(seDoor->joint->axisLocal.y   == doctest::Approx(1.0f));
    CHECK(seDoor->joint->limitMinDeg   == doctest::Approx(0.0f));
    CHECK(seDoor->joint->limitMaxDeg   == doctest::Approx(90.0f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: JointComponent Distance round-trip y default fields no persistidos") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity a = scene.createEntity("A");
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.halfExtents = glm::vec3(0.5f);
        a.addComponent<RigidBodyComponent>(rb);
    }
    Entity b = scene.createEntity("B");
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        rb.halfExtents = glm::vec3(0.5f);
        b.addComponent<RigidBodyComponent>(rb);
        JointComponent jc{};
        jc.type         = JointComponent::Type::Distance;
        jc.targetEntity = static_cast<u32>(a.handle());
        jc.minDistance  = 2.0f;
        jc.maxDistance  = 5.0f;
        b.addComponent<JointComponent>(jc);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("joint_distance.moodmap");
    SceneSerializer::save(empty, "rope", &scene, assets, path);

    // En el JSON crudo, axisLocal/limit_min/limit_max NO deben aparecer
    // (defaults del tipo Distance — solo se emiten para Hinge).
    std::ifstream f(path);
    REQUIRE(f.is_open());
    nlohmann::json j = nlohmann::json::parse(f);
    f.close();
    const nlohmann::json* jJoint = nullptr;
    for (const auto& jent : j["entities"]) {
        if (jent.value("tag", std::string{}) == "B"
            && jent.contains("joint")) {
            jJoint = &jent["joint"];
            break;
        }
    }
    REQUIRE(jJoint != nullptr);
    CHECK((*jJoint).value("type", std::string{}) == "distance");
    CHECK_FALSE((*jJoint).contains("axisLocal"));
    CHECK_FALSE((*jJoint).contains("limit_min_deg"));
    CHECK_FALSE((*jJoint).contains("limit_max_deg"));
    CHECK((*jJoint).value("min_distance", -1.0f) == doctest::Approx(2.0f));
    CHECK((*jJoint).value("max_distance", -1.0f) == doctest::Approx(5.0f));

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const SavedEntity* seB = nullptr;
    for (const auto& se : loaded->entities) if (se.tag == "B") seB = &se;
    REQUIRE(seB != nullptr);
    REQUIRE(seB->joint.has_value());
    CHECK(seB->joint->type == "distance");
    CHECK(seB->joint->targetTag == "A");
    CHECK(seB->joint->minDistance == doctest::Approx(2.0f));
    CHECK(seB->joint->maxDistance == doctest::Approx(5.0f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip JointComponent Slider (F2H71)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity rail = scene.createEntity("Riel");
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        rail.addComponent<RigidBodyComponent>(rb);
    }
    Entity drawer = scene.createEntity("Cajon");
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.mass = 3.0f;
        drawer.addComponent<RigidBodyComponent>(rb);
        JointComponent jc{};
        jc.type           = JointComponent::Type::Slider;
        jc.targetEntity   = static_cast<u32>(rail.handle());
        jc.axisLocal      = glm::vec3(1.0f, 0.0f, 0.0f);  // riel horizontal
        jc.sliderLimitMin = 0.0f;
        jc.sliderLimitMax = 0.5f;
        drawer.addComponent<JointComponent>(jc);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("joint_slider.moodmap");
    SceneSerializer::save(empty, "slider", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const SavedEntity* seD = nullptr;
    for (const auto& se : loaded->entities) if (se.tag == "Cajon") seD = &se;
    REQUIRE(seD != nullptr);
    REQUIRE(seD->joint.has_value());
    CHECK(seD->joint->type == "slider");
    CHECK(seD->joint->targetTag == "Riel");
    CHECK(seD->joint->axisLocal.x == doctest::Approx(1.0f));
    CHECK(seD->joint->sliderLimitMin == doctest::Approx(0.0f));
    CHECK(seD->joint->sliderLimitMax == doctest::Approx(0.5f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip JointComponent Fixed (F2H71)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity platform = scene.createEntity("Plataforma");
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        platform.addComponent<RigidBodyComponent>(rb);
    }
    Entity crate = scene.createEntity("Caja");
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.mass = 10.0f;
        crate.addComponent<RigidBodyComponent>(rb);
        JointComponent jc{};
        jc.type         = JointComponent::Type::Fixed;
        jc.targetEntity = static_cast<u32>(platform.handle());
        crate.addComponent<JointComponent>(jc);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("joint_fixed.moodmap");
    SceneSerializer::save(empty, "fixed", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const SavedEntity* seC = nullptr;
    for (const auto& se : loaded->entities) if (se.tag == "Caja") seC = &se;
    REQUIRE(seC != nullptr);
    REQUIRE(seC->joint.has_value());
    CHECK(seC->joint->type == "fixed");
    CHECK(seC->joint->targetTag == "Plataforma");

    std::filesystem::remove(path);
}

TEST_CASE("SceneLoader: JointComponent.targetEntity resuelve por tag tras applyEntitiesToScene (F2H65)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    // SavedMap con 2 entities: la PRIMERA tiene un joint apuntando a la
    // SEGUNDA (orden a-proposito: el eager lookup falla en applyOneEntity,
    // la segunda pasada del loader es la que resuelve).
    SavedMap saved{"test", GridMap(1u, 1u, 1.0f), {}};
    SavedEntity owner{};
    owner.tag = "Owner";
    {
        SavedRigidBody rb{};
        rb.type = "dynamic";
        owner.rigidBody = rb;
        SavedJoint j{};
        j.type        = "hinge";
        j.targetTag   = "Target";
        j.pivotLocal  = glm::vec3(-0.5f, 0.0f, 0.0f);
        owner.joint   = j;
    }
    SavedEntity target{};
    target.tag = "Target";
    {
        SavedRigidBody rb{};
        rb.type = "static";
        target.rigidBody = rb;
    }
    saved.entities.push_back(owner);
    saved.entities.push_back(target);

    SceneLoader::applyEntitiesToScene(saved, scene, assets);

    // Localizar las 2 entities y comprobar que el Joint del Owner resolvio.
    Entity ownerE, targetE;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (t.name == "Owner")  ownerE = e;
        if (t.name == "Target") targetE = e;
    });
    REQUIRE(static_cast<bool>(ownerE));
    REQUIRE(static_cast<bool>(targetE));
    REQUIRE(ownerE.hasComponent<JointComponent>());
    const auto& jc = ownerE.getComponent<JointComponent>();
    CHECK(jc.type == JointComponent::Type::Hinge);
    CHECK(jc.targetEntity == static_cast<u32>(targetE.handle()));
    CHECK(jc.pivotLocal.x == doctest::Approx(-0.5f));
    CHECK(jc.dirty);  // listo para materializar en el primer tick
}

TEST_CASE("SceneLoader: JointComponent con target_tag inexistente queda sin asignar (F2H65)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    SavedMap saved{"test", GridMap(1u, 1u, 1.0f), {}};
    SavedEntity orphan{};
    orphan.tag = "Lonely";
    SavedJoint j{};
    j.type      = "point";
    j.targetTag = "NoExisteEsteTag";
    orphan.joint = j;
    saved.entities.push_back(orphan);

    SceneLoader::applyEntitiesToScene(saved, scene, assets);

    Entity lonely;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (t.name == "Lonely") lonely = e;
    });
    REQUIRE(static_cast<bool>(lonely));
    REQUIRE(lonely.hasComponent<JointComponent>());
    const auto& jc = lonely.getComponent<JointComponent>();
    CHECK(jc.targetEntity == kJointNoTarget);  // tag no resolvio => sin asignar
    CHECK(jc.type == JointComponent::Type::Point);
}

TEST_CASE("SceneSerializer: sample map physics_joints_demo.moodmap carga con joints resueltos (F2H65 Bloque G)") {
    AssetManager assets("assets", nullFactory());
    const auto path = std::filesystem::path("assets") / "maps" /
                       "physics_joints_demo.moodmap";
    if (!std::filesystem::exists(path)) {
        MESSAGE("Sample map no encontrado en " << path.string()
                << " — skip (CWD probablemente fuera del repo root)");
        return;
    }
    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());

    // Las 4 entities estan: Marco/Puerta para Hinge, Soporte/Pendulo para
    // Distance. Cada par tiene UN joint en el dynamic.
    const SavedEntity* sePuerta = nullptr;
    const SavedEntity* sePendulo = nullptr;
    const SavedEntity* seAscensor = nullptr;  // F2H71 Slider
    const SavedEntity* seCaja = nullptr;       // F2H71 Fixed
    for (const auto& se : loaded->entities) {
        if (se.tag == "PuertaHinge")     sePuerta = &se;
        if (se.tag == "PenduloDistance") sePendulo = &se;
        if (se.tag == "AscensorSlider")  seAscensor = &se;
        if (se.tag == "CajaFixed")       seCaja = &se;
    }
    REQUIRE(sePuerta  != nullptr);
    REQUIRE(sePendulo != nullptr);
    REQUIRE(sePuerta->joint.has_value());
    REQUIRE(sePendulo->joint.has_value());
    CHECK(sePuerta->joint->type      == "hinge");
    CHECK(sePuerta->joint->targetTag == "MarcoHinge");
    CHECK(sePuerta->joint->limitMinDeg == doctest::Approx(-90.0f));
    CHECK(sePuerta->joint->limitMaxDeg == doctest::Approx(90.0f));
    CHECK(sePendulo->joint->type        == "distance");
    CHECK(sePendulo->joint->targetTag   == "SoporteDistance");
    CHECK(sePendulo->joint->minDistance == doctest::Approx(1.5f));
    CHECK(sePendulo->joint->maxDistance == doctest::Approx(1.5f));

    // F2H71: el demo trae ademas un Slider (ascensor) y un Fixed (caja soldada).
    REQUIRE(seAscensor != nullptr);
    REQUIRE(seCaja != nullptr);
    REQUIRE(seAscensor->joint.has_value());
    REQUIRE(seCaja->joint.has_value());
    CHECK(seAscensor->joint->type           == "slider");
    CHECK(seAscensor->joint->targetTag      == "RielSlider");
    CHECK(seAscensor->joint->sliderLimitMin == doctest::Approx(-3.0f));
    CHECK(seAscensor->joint->sliderLimitMax == doctest::Approx(0.0f));
    CHECK(seCaja->joint->type      == "fixed");
    CHECK(seCaja->joint->targetTag == "PlataformaFixed");

    // Pasada de SceneLoader: el targetEntity debe quedar resuelto via tag.
    Scene scene;
    SceneLoader::applyEntitiesToScene(*loaded, scene, assets);
    Entity puertaE, marcoE, penduloE, soporteE;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (t.name == "PuertaHinge")      puertaE  = e;
        if (t.name == "MarcoHinge")       marcoE   = e;
        if (t.name == "PenduloDistance")  penduloE = e;
        if (t.name == "SoporteDistance")  soporteE = e;
    });
    REQUIRE(static_cast<bool>(puertaE));
    REQUIRE(static_cast<bool>(marcoE));
    REQUIRE(static_cast<bool>(penduloE));
    REQUIRE(static_cast<bool>(soporteE));
    REQUIRE(puertaE.hasComponent<JointComponent>());
    REQUIRE(penduloE.hasComponent<JointComponent>());
    CHECK(puertaE.getComponent<JointComponent>().targetEntity
          == static_cast<u32>(marcoE.handle()));
    CHECK(penduloE.getComponent<JointComponent>().targetEntity
          == static_cast<u32>(soporteE.handle()));
}

TEST_CASE("SceneSerializer: friction default (0.5) NO se persiste en JSON (Hito 34 A)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity box = scene.createEntity("Caja");
    {
        RigidBodyComponent rb{};
        rb.type        = RigidBodyComponent::Type::Dynamic;
        rb.shape       = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(0.5f);
        rb.mass        = 1.0f;
        box.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("friction_default.moodmap");
    SceneSerializer::save(empty, "default", &scene, assets, path);

    std::ifstream f(path);
    REQUIRE(f.is_open());
    nlohmann::json j = nlohmann::json::parse(f);
    f.close();
    REQUIRE(j.contains("entities"));
    REQUIRE(j["entities"].size() >= 1u);
    const auto& jent = j["entities"][0];
    REQUIRE(jent.contains("rigid_body"));
    CHECK_FALSE(jent["rigid_body"].contains("friction"));

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() >= 1u);
    REQUIRE(loaded->entities[0].rigidBody.has_value());
    CHECK(loaded->entities[0].rigidBody->friction == doctest::Approx(0.5f));

    std::filesystem::remove(path);
}


// F2H67 Bloque J: round-trip persistencia de VehicleComponent + Seat.
// Verifica que `configPath` se serializa y rehidrata + que los handles
// runtime (vehicleId, wheelEntities[]) NO se persisten + que el dirty=true
// queda seteado al cargar (para que el VehicleSystem lo materialice).
TEST_CASE("SceneSerializer: round-trip VehicleComponent (F2H67)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity chassis = scene.createEntity("Banshee");
    {
        auto& t = chassis.getComponent<TransformComponent>();
        t.position = glm::vec3(2.0f, 1.0f, 5.0f);
        t.rotationEuler = glm::vec3(0.0f, 45.0f, 0.0f);
        VehicleComponent veh{};
        veh.configPath = "vehicles/banshee_sa.moodvehicle";
        // Setear handles runtime para verificar que NO persisten.
        veh.vehicleId = 99;
        veh.wheelEntities[0] = 100;
        veh.wheelEntities[1] = 101;
        veh.wheelEntities[2] = 102;
        veh.wheelEntities[3] = 103;
        chassis.addComponent<VehicleComponent>(veh);
        VehicleSeatComponent seat{};
        seat.seatOffsetLocal = glm::vec3(0.1f, 0.7f, 0.3f);
        chassis.addComponent<VehicleSeatComponent>(seat);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("vehicle_roundtrip.moodmap");
    SceneSerializer::save(empty, "vehicle_test", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() >= 1u);
    const SavedEntity& se = loaded->entities[0];
    CHECK(se.tag == "Banshee");
    REQUIRE(se.vehicle.has_value());
    CHECK(se.vehicle->configPath == "vehicles/banshee_sa.moodvehicle");
    REQUIRE(se.vehicleSeat.has_value());
    CHECK(se.vehicleSeat->seatOffsetLocal.x == doctest::Approx(0.1f));
    CHECK(se.vehicleSeat->seatOffsetLocal.y == doctest::Approx(0.7f));
    CHECK(se.vehicleSeat->seatOffsetLocal.z == doctest::Approx(0.3f));

    // SceneLoader::applyEntitiesToScene rehidrata. dirty debe quedar true
    // para que VehicleSystem materialice; vehicleId+wheelEntities deben
    // arrancar en 0 (handles runtime NO persistidos).
    Scene scene2;
    SceneLoader::applyEntitiesToScene(*loaded, scene2, assets);
    Entity reloaded;
    scene2.forEach<TagComponent>([&](Entity e, TagComponent& t) {
        if (t.name == "Banshee") reloaded = e;
    });
    REQUIRE(static_cast<bool>(reloaded));
    REQUIRE(reloaded.hasComponent<VehicleComponent>());
    const auto& vehR = reloaded.getComponent<VehicleComponent>();
    CHECK(vehR.configPath == "vehicles/banshee_sa.moodvehicle");
    CHECK(vehR.vehicleId == 0u);
    CHECK(vehR.wheelEntities[0] == 0u);
    CHECK(vehR.wheelEntities[1] == 0u);
    CHECK(vehR.wheelEntities[2] == 0u);
    CHECK(vehR.wheelEntities[3] == 0u);
    CHECK(vehR.dirty);
    REQUIRE(reloaded.hasComponent<VehicleSeatComponent>());
    CHECK(reloaded.getComponent<VehicleSeatComponent>().seatOffsetLocal.y
            == doctest::Approx(0.7f));

    std::filesystem::remove(path);
}

// F2H70.3 H: las wheel-entities que spawnea el VehicleSystem tienen
// MeshRenderer, pero NO deben serializarse: el VehicleSystem las
// rematerializa al cargar. Si se persistieran, recargar dejaria 4 ruedas
// huerfanas + 4 respawneadas = 8.
// F2H82: el skip se hace por componente `VehicleWheelMarker` (no por tag),
// asi cubre tambien autos importados con nombres de rueda no canonicos
// (ej. `f_t_l`, `RUEDA_*`).
TEST_CASE("SceneSerializer: wheel-entities no se serializan (F2H70.3)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity chassis = scene.createEntity("Banshee");
    VehicleComponent veh{};
    veh.configPath = "vehicles/banshee_sa.moodvehicle";
    chassis.addComponent<VehicleComponent>(veh);
    const u32 chassisHandle = static_cast<u32>(chassis.handle());

    // 4 wheel-entities como las crea VehicleSystem::spawnPendingWheels:
    // tag canonico + MeshRenderer + VehicleWheelMarker (marker que el
    // serializer usa para reconocer y skipear estas entities).
    const char* names[4] = {"wheel_FL", "wheel_FR", "wheel_RL", "wheel_RR"};
    for (int i = 0; i < 4; ++i) {
        Entity w = scene.createEntity(names[i]);
        w.addComponent<MeshRendererComponent>(0u, 0u);
        VehicleWheelMarker mk{};
        mk.chassisHandle = chassisHandle;
        mk.wheelIndex = i;
        w.addComponent<VehicleWheelMarker>(mk);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("vehicle_wheels_skip.moodmap");
    SceneSerializer::save(empty, "wheel_skip_test", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    // Solo el chassis debe persistir; ninguna entity con tag wheel_*.
    for (const SavedEntity& e : loaded->entities) {
        CHECK(e.tag != "wheel_FL");
        CHECK(e.tag != "wheel_FR");
        CHECK(e.tag != "wheel_RL");
        CHECK(e.tag != "wheel_RR");
    }
    bool sawChassis = false;
    for (const SavedEntity& e : loaded->entities) {
        if (e.tag == "Banshee") sawChassis = true;
    }
    CHECK(sawChassis);

    std::filesystem::remove(path);
}

// F2H67 Bloque J: VehicleComponent es opcional (aditivo). Mapas pre-F2H67
// sin el campo "vehicle" cargan igual y la entity no tiene el componente.
TEST_CASE("SceneSerializer: mapas pre-F2H67 sin VehicleComponent cargan (F2H67)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;
    // Entity sin VehicleComponent pero con RigidBody (para que pase el gate
    // de serializacion). Verificamos que el JSON NO trae campo "vehicle".
    Entity simple = scene.createEntity("SinAuto");
    simple.getComponent<TransformComponent>().position = glm::vec3(0.0f);
    {
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(1.0f);
        simple.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("vehicle_absent.moodmap");
    SceneSerializer::save(empty, "no_vehicle", &scene, assets, path);

    // Reabrir como JSON y verificar que NO se escribio el campo vehicle.
    std::ifstream f(path);
    REQUIRE(f.is_open());
    nlohmann::json j = nlohmann::json::parse(f);
    f.close();
    REQUIRE(j.contains("entities"));
    REQUIRE(j["entities"].size() >= 1u);
    CHECK_FALSE(j["entities"][0].contains("vehicle"));

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() >= 1u);
    CHECK_FALSE(loaded->entities[0].vehicle.has_value());

    std::filesystem::remove(path);
}
