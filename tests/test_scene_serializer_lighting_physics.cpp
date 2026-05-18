// Tests de SceneSerializer — split AUDIT-3 (familia: lighting + physics).
// Roundtrip de LightComponent (Hito 11, 16) + RigidBodyComponent
// (Hito 12, 34 A). Comparte helpers con test_scene_serializer.cpp via
// test_scene_serializer_helpers.h.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
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
