// Tests del ForceFieldSystem (F2H72). Cubren:
//   - Directional empuja al body en su direccion.
//   - Radial (strength>0) aleja al body del centro.
//   - Un body fuera de la zona no recibe fuerza.
//   - enabled==false no aplica nada.
//
// Las aserciones miran el eje X (horizontal): la gravedad solo afecta Y, asi
// que un cambio en X es prueba directa de que la fuerza de la zona actuo.

#include <doctest/doctest.h>

#include "engine/physics/world/PhysicsWorld.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "systems/physics/ForceFieldSystem.h"

#include <glm/geometric.hpp>

using namespace Mood;

namespace {

constexpr f32 k_dt = 0.016f;

// Crea un body dynamic en `pos` + una entity con RigidBodyComponent cuyo
// bodyId apunta a ese body (el ForceFieldSystem itera por RigidBodyComponent
// y lee la posicion viva via physics.bodyPosition).
u32 spawnDynamic(Scene& scene, PhysicsWorld& pw, const glm::vec3& pos) {
    const u32 bid = pw.createBody(pos, CollisionShape::Box, glm::vec3(0.25f),
                                   BodyType::Dynamic, 1.0f);
    Entity e = scene.createEntity("body");
    e.getComponent<TransformComponent>().position = pos;
    RigidBodyComponent rb{};
    rb.type = RigidBodyComponent::Type::Dynamic;
    rb.bodyId = bid;
    rb.mass = 1.0f;
    e.addComponent<RigidBodyComponent>(rb);
    return bid;
}

// Avanza N frames aplicando el campo antes de cada step (las fuerzas que Jolt
// acumula se aplican en el step y se limpian despues).
void run(Scene& scene, PhysicsWorld& pw, ForceFieldSystem& sys, int frames) {
    for (int i = 0; i < frames; ++i) {
        sys.update(scene, pw, k_dt);
        pw.step(k_dt);
    }
}

} // namespace

TEST_CASE("ForceFieldSystem: Directional empuja al body en su direccion") {
    Scene scene;
    PhysicsWorld pw;
    const u32 body = spawnDynamic(scene, pw, glm::vec3(0, 5, 0));

    Entity field = scene.createEntity("field");
    field.getComponent<TransformComponent>().position = glm::vec3(0, 5, 0);
    ForceFieldComponent ff{};
    ff.shape = ForceFieldComponent::Shape::Sphere;
    ff.radius = 10.0f;
    ff.mode = ForceFieldComponent::Mode::Directional;
    ff.direction = glm::vec3(1, 0, 0);
    ff.strength = 50.0f;
    field.addComponent<ForceFieldComponent>(ff);

    ForceFieldSystem sys;
    run(scene, pw, sys, 30);

    // Empujado en +X (la gravedad no toca X).
    CHECK(pw.bodyPosition(body).x > 0.5f);
}

TEST_CASE("ForceFieldSystem: Radial con strength>0 aleja al body del centro") {
    Scene scene;
    PhysicsWorld pw;
    const u32 body = spawnDynamic(scene, pw, glm::vec3(1, 5, 0));  // offset +X del centro

    Entity field = scene.createEntity("field");
    field.getComponent<TransformComponent>().position = glm::vec3(0, 5, 0);
    ForceFieldComponent ff{};
    ff.shape = ForceFieldComponent::Shape::Sphere;
    ff.radius = 10.0f;
    ff.mode = ForceFieldComponent::Mode::Radial;
    ff.strength = 50.0f;            // + = empuja afuera
    ff.linearFalloff = false;       // fuerza pareja en toda la zona
    field.addComponent<ForceFieldComponent>(ff);

    const f32 startX = pw.bodyPosition(body).x;
    ForceFieldSystem sys;
    run(scene, pw, sys, 30);

    // El body arranca a +X del centro -> la fuerza radial lo empuja mas lejos.
    CHECK(pw.bodyPosition(body).x > startX + 0.5f);
}

TEST_CASE("ForceFieldSystem: body fuera de la zona no recibe fuerza") {
    Scene scene;
    PhysicsWorld pw;
    const u32 body = spawnDynamic(scene, pw, glm::vec3(5, 5, 0));  // lejos del centro

    Entity field = scene.createEntity("field");
    field.getComponent<TransformComponent>().position = glm::vec3(0, 5, 0);
    ForceFieldComponent ff{};
    ff.shape = ForceFieldComponent::Shape::Sphere;
    ff.radius = 1.0f;               // zona chica, el body queda afuera
    ff.mode = ForceFieldComponent::Mode::Directional;
    ff.direction = glm::vec3(1, 0, 0);
    ff.strength = 50.0f;
    field.addComponent<ForceFieldComponent>(ff);

    ForceFieldSystem sys;
    run(scene, pw, sys, 30);

    // Sin fuerza horizontal -> X no cambia (cae en Y por gravedad nomas).
    CHECK(pw.bodyPosition(body).x == doctest::Approx(5.0f).epsilon(0.05));
}

TEST_CASE("ForceFieldSystem: enabled==false no aplica fuerza") {
    Scene scene;
    PhysicsWorld pw;
    const u32 body = spawnDynamic(scene, pw, glm::vec3(0, 5, 0));

    Entity field = scene.createEntity("field");
    field.getComponent<TransformComponent>().position = glm::vec3(0, 5, 0);
    ForceFieldComponent ff{};
    ff.shape = ForceFieldComponent::Shape::Sphere;
    ff.radius = 10.0f;
    ff.mode = ForceFieldComponent::Mode::Directional;
    ff.direction = glm::vec3(1, 0, 0);
    ff.strength = 50.0f;
    ff.enabled = false;             // apagado
    field.addComponent<ForceFieldComponent>(ff);

    ForceFieldSystem sys;
    run(scene, pw, sys, 30);

    CHECK(pw.bodyPosition(body).x == doctest::Approx(0.0f).epsilon(0.05));
}
