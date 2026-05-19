// F2H67 Bloque C/J: tests del wrapper PhysicsWorld_Vehicle (Jolt vehicle
// constraint + WheeledVehicleController). Smoke + behavior tests sobre el
// runtime: createVehicle handle, input -> velocity, gravity, etc.

#include <doctest/doctest.h>

#include "engine/physics/vehicle/VehicleConfig.h"
#include "engine/physics/world/PhysicsWorld.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>

using namespace Mood;
using namespace Mood::vehicle;

namespace {

// Helper: simula el world durante N ticks (60 FPS) y deja al vehicle
// asentarse en el suelo.
void stepN(PhysicsWorld& w, int n, f32 dt = 1.0f / 60.0f) {
    for (int i = 0; i < n; ++i) w.step(dt);
}

// Helper: crea un piso static a y=0 para que el vehicle caiga sobre algo.
u32 makeGround(PhysicsWorld& w) {
    return w.createBody(glm::vec3(0.0f, -0.5f, 0.0f),
                          CollisionShape::Box,
                          glm::vec3(50.0f, 0.5f, 50.0f),
                          BodyType::Static);
}

} // anon

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: createVehicle handle valido") {
    PhysicsWorld w;
    const u32 ground = makeGround(w);
    (void)ground;
    const VehicleConfig cfg = makeDefaultSA();
    // Arrancamos el vehicle a 2m sobre el piso (suspension lo asienta).
    const glm::mat4 xform = glm::translate(glm::mat4(1.0f),
                                            glm::vec3(0.0f, 2.0f, 0.0f));
    const u32 id = w.createVehicle(cfg, xform);
    CHECK(id != 0);
    CHECK(w.vehicleCount() == 1);
    CHECK(w.vehicleChassisBodyId(id) != 0);
}

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: createVehicle config invalido -> 0") {
    PhysicsWorld w;
    VehicleConfig cfg = makeDefaultSA();
    cfg.chassisMass = -1.0f;  // invalido
    const u32 id = w.createVehicle(cfg, glm::mat4(1.0f));
    CHECK(id == 0);
    CHECK(w.vehicleCount() == 0);
}

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: destroyVehicle idempotente") {
    PhysicsWorld w;
    const u32 ground = makeGround(w);
    (void)ground;
    const VehicleConfig cfg = makeDefaultSA();
    const u32 id = w.createVehicle(cfg,
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f)));
    REQUIRE(id != 0);
    w.destroyVehicle(id);
    CHECK(w.vehicleCount() == 0);
    w.destroyVehicle(id);  // idempotente
    CHECK(w.vehicleCount() == 0);
    w.destroyVehicle(0);   // id invalido
    CHECK(w.vehicleCount() == 0);
}

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: readVehicleState luego de settle") {
    PhysicsWorld w;
    const u32 ground = makeGround(w);
    (void)ground;
    const VehicleConfig cfg = makeDefaultSA();
    const u32 id = w.createVehicle(cfg,
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f)));
    REQUIRE(id != 0);

    // Sin input — solo dejamos que la suspension se asiente.
    stepN(w, 120);  // 2 segundos

    PhysicsWorld::VehicleState st;
    REQUIRE(w.readVehicleState(id, st));
    // El vehiculo no deberia haber caido del mapa. Y debe estar cerca
    // del suelo, no a 2m.
    CHECK(st.chassisWorld[3].y < 2.0f);
    CHECK(st.chassisWorld[3].y > -1.0f);
    // forwardSpeed ~ 0 sin input.
    CHECK(std::abs(st.forwardSpeed) < 0.5f);
    // Al menos 1 wheel sobre el suelo tras asentar.
    CHECK(st.grounded);
}

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: throttle -> forward speed > 0") {
    PhysicsWorld w;
    const u32 ground = makeGround(w);
    (void)ground;
    const VehicleConfig cfg = makeDefaultSA();
    const u32 id = w.createVehicle(cfg,
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f)));
    REQUIRE(id != 0);

    // Settle 1 segundo, luego throttle full.
    stepN(w, 60);
    // Aplicar throttle cada frame durante 3 segundos -- Jolt requiere
    // input persistente, no event-based.
    for (int i = 0; i < 180; ++i) {
        w.setVehicleInput(id, /*throttle*/1.0f, /*brake*/0.0f,
                            /*steer*/0.0f, /*handbrake*/0.0f);
        w.step(1.0f / 60.0f);
    }

    PhysicsWorld::VehicleState st;
    REQUIRE(w.readVehicleState(id, st));
    // Tras 3s de throttle full sobre piso plano, el sedan deberia haber
    // pasado los 5 m/s (~18 km/h). Margen conservador.
    CHECK(st.forwardSpeed > 5.0f);
}

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: brake decrece forward speed") {
    PhysicsWorld w;
    const u32 ground = makeGround(w);
    (void)ground;
    const VehicleConfig cfg = makeDefaultSA();
    const u32 id = w.createVehicle(cfg,
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f)));
    REQUIRE(id != 0);

    stepN(w, 60);  // settle
    // Acelerar 2s.
    for (int i = 0; i < 120; ++i) {
        w.setVehicleInput(id, 1.0f, 0.0f, 0.0f, 0.0f);
        w.step(1.0f / 60.0f);
    }
    PhysicsWorld::VehicleState before;
    REQUIRE(w.readVehicleState(id, before));
    REQUIRE(before.forwardSpeed > 3.0f);

    // Brake 2s.
    for (int i = 0; i < 120; ++i) {
        w.setVehicleInput(id, 0.0f, 1.0f, 0.0f, 0.0f);
        w.step(1.0f / 60.0f);
    }
    PhysicsWorld::VehicleState after;
    REQUIRE(w.readVehicleState(id, after));
    CHECK(after.forwardSpeed < before.forwardSpeed);
}

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: applyVehicleImpulse mueve el chassis") {
    PhysicsWorld w;
    const u32 ground = makeGround(w);
    (void)ground;
    const VehicleConfig cfg = makeDefaultSA();
    const u32 id = w.createVehicle(cfg,
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f)));
    REQUIRE(id != 0);
    stepN(w, 60);  // settle

    PhysicsWorld::VehicleState before;
    REQUIRE(w.readVehicleState(id, before));

    // Impulse fuerte hacia +Z. Para un sedan 1500kg en reposo, 30000 N*s
    // (=> 20 m/s instantaneo) deberia notarse.
    w.applyVehicleImpulse(id, glm::vec3(0.0f, 0.0f, 30000.0f));
    stepN(w, 5);  // 5 ticks para que la velocidad propague al transform

    PhysicsWorld::VehicleState after;
    REQUIRE(w.readVehicleState(id, after));
    CHECK(after.forwardSpeed > before.forwardSpeed + 2.0f);
}

TEST_CASE("PhysicsWorld_Vehicle F2H67 C: multi-vehicle handles distintos") {
    PhysicsWorld w;
    const u32 ground = makeGround(w);
    (void)ground;
    const VehicleConfig cfg = makeDefaultSA();
    const u32 a = w.createVehicle(cfg,
        glm::translate(glm::mat4(1.0f), glm::vec3(-5.0f, 2.0f, 0.0f)));
    const u32 b = w.createVehicle(cfg,
        glm::translate(glm::mat4(1.0f), glm::vec3( 5.0f, 2.0f, 0.0f)));
    REQUIRE(a != 0);
    REQUIRE(b != 0);
    CHECK(a != b);
    CHECK(w.vehicleCount() == 2);
    // Destruir uno NO afecta al otro.
    w.destroyVehicle(a);
    CHECK(w.vehicleCount() == 1);
    PhysicsWorld::VehicleState st;
    CHECK(w.readVehicleState(b, st));
}
