// Tests de pickEntity (Hito 13 Bloque 1). Smoke + invariantes:
//   - scene vacia -> sin hit
//   - mesh atravesado por el rayo -> hit con la entidad correcta
//   - light/audio sin mesh -> hit por la esfera de pickeo
//   - dos targets en la linea del rayo -> el mas cercano gana
//   - entidad detras de la camara -> sin hit

#include <doctest/doctest.h>

#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/scene/ScenePick.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

using namespace Mood;

namespace {

/// Camara mirando desde +Z hacia el origen, plano XY como "pantalla".
/// Centro de NDC (0,0) = direccion -Z desde (0,0,5).
struct TestCamera {
    glm::mat4 view;
    glm::mat4 projection;
};

TestCamera makeCamera() {
    TestCamera c;
    c.view = glm::lookAt(glm::vec3(0, 0, 5),
                          glm::vec3(0, 0, 0),
                          glm::vec3(0, 1, 0));
    c.projection = glm::perspective(glm::radians(60.0f), 1.0f, 0.1f, 100.0f);
    return c;
}

} // namespace

TEST_CASE("pickEntity: scene vacia no devuelve hit") {
    Scene s;
    const auto cam = makeCamera();
    const auto r = pickEntity(s, cam.view, cam.projection, glm::vec2(0, 0));
    CHECK_FALSE(static_cast<bool>(r));
    CHECK_FALSE(static_cast<bool>(r.entity));
}

TEST_CASE("pickEntity: mesh en el origen es hiteado por rayo central") {
    Scene s;
    Entity e = s.createEntity("cubo");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0, 0, 0);
    t.scale = glm::vec3(1.0f);
    e.addComponent<MeshRendererComponent>();

    const auto cam = makeCamera();
    const auto r = pickEntity(s, cam.view, cam.projection, glm::vec2(0, 0));
    REQUIRE(static_cast<bool>(r));
    CHECK(r.entity == e);
    CHECK(r.distance > 0.0f);
}

TEST_CASE("pickEntity: light sin mesh es pickable por su esfera (radius 0.6m)") {
    Scene s;
    Entity light = s.createEntity("luz");
    auto& t = light.getComponent<TransformComponent>();
    t.position = glm::vec3(0, 0, 0);
    LightComponent lc{};
    lc.type = LightComponent::Type::Point;
    light.addComponent<LightComponent>(lc);

    const auto cam = makeCamera();
    const auto r = pickEntity(s, cam.view, cam.projection, glm::vec2(0, 0));
    REQUIRE(static_cast<bool>(r));
    CHECK(r.entity == light);
}

TEST_CASE("pickEntity: dos entidades en linea del rayo, el mas cercano gana") {
    Scene s;
    Entity nearE = s.createEntity("near");
    {
        auto& t = nearE.getComponent<TransformComponent>();
        t.position = glm::vec3(0, 0, 1);  // mas cerca de la camara (z=5)
        t.scale = glm::vec3(1.0f);
        nearE.addComponent<MeshRendererComponent>();
    }
    Entity farE = s.createEntity("far");
    {
        auto& t = farE.getComponent<TransformComponent>();
        t.position = glm::vec3(0, 0, -2); // mas lejos
        t.scale = glm::vec3(1.0f);
        farE.addComponent<MeshRendererComponent>();
    }

    const auto cam = makeCamera();
    const auto r = pickEntity(s, cam.view, cam.projection, glm::vec2(0, 0));
    REQUIRE(static_cast<bool>(r));
    CHECK(r.entity == nearE);
}

TEST_CASE("pickEntity: rayo en el cielo (NDC arriba) no hitea nada en el origen") {
    Scene s;
    Entity e = s.createEntity("cubo");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0, 0, 0);
    t.scale = glm::vec3(1.0f);
    e.addComponent<MeshRendererComponent>();

    const auto cam = makeCamera();
    // ndc (0, 0.95) = casi en el tope; el rayo apunta muy arriba y no
    // toca el cubo unitario.
    const auto r = pickEntity(s, cam.view, cam.projection, glm::vec2(0.0f, 0.95f));
    CHECK_FALSE(static_cast<bool>(r));
}

TEST_CASE("pickEntity: entidad detras de la camara no hitea") {
    Scene s;
    Entity e = s.createEntity("atras");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(0, 0, 10); // detras de la camara (z=5 mira a -z)
    t.scale = glm::vec3(1.0f);
    e.addComponent<MeshRendererComponent>();

    const auto cam = makeCamera();
    const auto r = pickEntity(s, cam.view, cam.projection, glm::vec2(0, 0));
    CHECK_FALSE(static_cast<bool>(r));
}

TEST_CASE("pickEntity: entidad sin Transform no hitea (safety)") {
    Scene s;
    // Una entidad creada manualmente via registry sin Transform — no la
    // creamos via Scene::createEntity (que la agrega por default), sino
    // por raw entt para comprobar el guard.
    auto& reg = s.registry();
    auto raw = reg.create();
    reg.emplace<TagComponent>(raw, "raw_no_tform");
    // Sin TransformComponent, sin MeshRenderer, sin Light, sin Audio.

    const auto cam = makeCamera();
    const auto r = pickEntity(s, cam.view, cam.projection, glm::vec2(0, 0));
    CHECK_FALSE(static_cast<bool>(r));
}
