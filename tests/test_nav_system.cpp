// Tests del NavSystem: integrador entre NavAgentComponent + GridMap +
// Pathfinding (Hito 23 Bloque 6). Headless: solo Scene + GridMap +
// NavSystem; sin GL ni AssetManager.

#include <doctest/doctest.h>

#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/world/GridMap.h"
#include "systems/NavSystem.h"

#include <cmath>

using namespace Mood;

namespace {

// Helper: crea un agente en (x, z) world-space, con target.
Entity makeAgent(Scene& scene, const glm::vec3& pos, const glm::vec3& target) {
    Entity e = scene.createEntity("agent");
    e.getComponent<TransformComponent>().position = pos;
    NavAgentComponent nav{};
    nav.target = target;
    nav.speed  = 5.0f; // alto para converger en pocos frames
    nav.active = true;
    e.addComponent<NavAgentComponent>(nav);
    return e;
}

// Avanza N frames con dt fijo, manteniendo target estable.
void simulate(NavSystem& nav, Scene& scene, GridMap& map,
              const glm::vec3& origin, int frames, f32 dt = 0.05f) {
    for (int i = 0; i < frames; ++i) nav.update(scene, map, origin, dt);
}

f32 distXZ(const glm::vec3& a, const glm::vec3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}

} // namespace

TEST_CASE("NavSystem: agente avanza hacia target en sala vacia") {
    GridMap map(5u, 5u);
    Scene scene;
    NavSystem nav;
    const glm::vec3 origin(0.0f);

    // Agente en (1.5, 0, 1.5) → target en (4.5, 0, 4.5). Sala vacia.
    const glm::vec3 startPos(1.5f, 0.0f, 1.5f);
    const glm::vec3 target(4.5f, 0.0f, 4.5f);
    Entity agent = makeAgent(scene, startPos, target);

    const f32 distBefore = distXZ(startPos, target);
    simulate(nav, scene, map, origin, 60); // 3 segundos de sim
    const auto& tf = agent.getComponent<TransformComponent>();
    const f32 distAfter = distXZ(tf.position, target);

    CHECK(distAfter < distBefore); // se acerco
}

TEST_CASE("NavSystem: agente llega al destino y se queda quieto") {
    GridMap map(5u, 5u);
    Scene scene;
    NavSystem nav;
    const glm::vec3 origin(0.0f);

    const glm::vec3 startPos(0.5f, 0.0f, 0.5f);
    const glm::vec3 target(2.5f, 0.0f, 2.5f);
    Entity agent = makeAgent(scene, startPos, target);

    simulate(nav, scene, map, origin, 200); // largo: que llegue seguro
    const glm::vec3 posAfterArrival =
        agent.getComponent<TransformComponent>().position;

    // 50 frames mas — no deberia moverse mas alla de margen flotante.
    simulate(nav, scene, map, origin, 50);
    const glm::vec3 posLater = agent.getComponent<TransformComponent>().position;

    CHECK(distXZ(posAfterArrival, posLater) < 0.05f);
}

TEST_CASE("NavSystem: target inalcanzable -> agente no se mueve") {
    GridMap map(5u, 5u);
    // Encerramos el goal con muros en sus 4 vecinos.
    map.setTile(3u, 4u, TileType::SolidWall);
    map.setTile(4u, 3u, TileType::SolidWall);
    // (5,4) y (4,5) ya son fuera del mapa = solidos.

    Scene scene;
    NavSystem nav;
    const glm::vec3 origin(0.0f);

    const glm::vec3 startPos(0.5f, 0.0f, 0.5f);
    const glm::vec3 target(13.5f, 0.0f, 13.5f); // tile (4,4) — encerrado
    Entity agent = makeAgent(scene, startPos, target);

    simulate(nav, scene, map, origin, 50);

    const glm::vec3 posAfter = agent.getComponent<TransformComponent>().position;
    CHECK(distXZ(posAfter, startPos) < 0.1f);
}

TEST_CASE("NavSystem: agente inactivo no se mueve") {
    GridMap map(5u, 5u);
    Scene scene;
    NavSystem nav;
    const glm::vec3 origin(0.0f);

    const glm::vec3 startPos(0.5f, 0.0f, 0.5f);
    Entity agent = makeAgent(scene, startPos, glm::vec3(4.5f, 0.0f, 4.5f));
    agent.getComponent<NavAgentComponent>().active = false;

    simulate(nav, scene, map, origin, 30);
    const glm::vec3 posAfter = agent.getComponent<TransformComponent>().position;
    CHECK(distXZ(posAfter, startPos) < 0.01f);
}
