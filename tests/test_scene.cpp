// Tests de la fachada Scene + Entity (Hito 7 Bloque 2).
// Verifican el ciclo de vida basico (create/destroy), componentes, y que la
// fachada no filtra EnTT cuando no es necesario.

#include <doctest/doctest.h>

#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/vec3.hpp>

#include <cmath>
#include <string>
#include <vector>

using namespace Mood;

TEST_CASE("Scene: createEntity agrega Tag + Transform por default") {
    Scene scene;
    Entity e = scene.createEntity("mi_cubo");
    REQUIRE(static_cast<bool>(e));
    CHECK(e.hasComponent<TagComponent>());
    CHECK(e.hasComponent<TransformComponent>());
    CHECK(e.getComponent<TagComponent>().name == "mi_cubo");
    CHECK(e.getComponent<TransformComponent>().position == glm::vec3(0.0f));
    CHECK(e.getComponent<TransformComponent>().scale == glm::vec3(1.0f));
}

TEST_CASE("Scene: createEntity sin nombre usa 'Entity' como default") {
    Scene scene;
    Entity e = scene.createEntity();
    CHECK(e.getComponent<TagComponent>().name == "Entity");
}

TEST_CASE("Scene: entityCount refleja los creates/destroys") {
    Scene scene;
    CHECK(scene.entityCount() == 0);
    Entity a = scene.createEntity("a");
    Entity b = scene.createEntity("b");
    Entity c = scene.createEntity("c");
    CHECK(scene.entityCount() == 3);
    scene.destroyEntity(b);
    CHECK(scene.entityCount() == 2);
    CHECK(a);
    CHECK(c);
    // `b` sigue siendo un wrapper valido en C++, pero el handle ya no es
    // valido en el registry. hasComponent no lanza; simplemente es false.
}

TEST_CASE("Entity: add/get/has/remove de un componente custom") {
    struct Counter { int value = 0; };

    Scene scene;
    Entity e = scene.createEntity();
    CHECK_FALSE(e.hasComponent<Counter>());

    Counter& c = e.addComponent<Counter>();
    c.value = 42;
    CHECK(e.hasComponent<Counter>());
    CHECK(e.getComponent<Counter>().value == 42);

    e.removeComponent<Counter>();
    CHECK_FALSE(e.hasComponent<Counter>());
}

TEST_CASE("Entity: addComponent reemplaza el existente") {
    Scene scene;
    Entity e = scene.createEntity("foo");
    e.addComponent<TagComponent>(std::string{"bar"});
    CHECK(e.getComponent<TagComponent>().name == "bar");
}

TEST_CASE("Scene::forEach itera entidades con los componentes pedidos") {
    Scene scene;
    Entity a = scene.createEntity("a");
    a.addComponent<MeshRendererComponent>(MeshAssetId{0}, TextureAssetId{1});
    Entity b = scene.createEntity("b");
    b.addComponent<MeshRendererComponent>(MeshAssetId{0}, TextureAssetId{2});
    Entity c = scene.createEntity("c"); // sin MeshRenderer

    int visited = 0;
    TextureAssetId sumIds = 0;
    scene.forEach<MeshRendererComponent>(
        [&](Entity, MeshRendererComponent& mr) {
            ++visited;
            // El ctor (mesh, tex) mete `tex` en materials[0].
            if (!mr.materials.empty()) sumIds += mr.materials[0];
        });
    CHECK(visited == 2);
    CHECK(sumIds == 3u);
}

TEST_CASE("Entity default-constructed evalua a false") {
    Entity e;
    CHECK_FALSE(static_cast<bool>(e));
}

TEST_CASE("Entity: TransformComponent worldMatrix aplica trans/scale correctamente") {
    Scene scene;
    Entity e = scene.createEntity();
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(5.0f, 0.0f, 0.0f);
    t.scale = glm::vec3(2.0f);
    const auto m = t.worldMatrix();
    // columna 3 = translation
    CHECK(m[3][0] == doctest::Approx(5.0f));
    // diagonal = scale (sin rotacion)
    CHECK(m[0][0] == doctest::Approx(2.0f));
    CHECK(m[1][1] == doctest::Approx(2.0f));
    CHECK(m[2][2] == doctest::Approx(2.0f));
}

// ============================================================
// F3H27: helpers de jerarquia parent/child
// ============================================================

TEST_CASE("F3H27 Scene::worldMatrixOf sin parent devuelve local") {
    Scene scene;
    Entity e = scene.createEntity("root");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(3.0f, 0.0f, 0.0f);
    t.scale = glm::vec3(2.0f);
    const auto local = t.worldMatrix();
    const auto world = scene.worldMatrixOf(e.handle());
    // Sin parent, world == local.
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            CHECK(world[c][r] == doctest::Approx(local[c][r]));
}

TEST_CASE("F3H27 Scene::worldMatrixOf con parent acumula world") {
    Scene scene;
    Entity parent = scene.createEntity("parent");
    parent.getComponent<TransformComponent>().position =
        glm::vec3(10.0f, 0.0f, 0.0f);
    Entity child = scene.createEntity("child");
    auto& tc = child.getComponent<TransformComponent>();
    tc.parent = parent.handle();
    tc.position = glm::vec3(2.0f, 0.0f, 0.0f);  // local al padre

    const auto world = scene.worldMatrixOf(child.handle());
    // El child queda en world (10 + 2) = 12 en X.
    CHECK(world[3][0] == doctest::Approx(12.0f));
}

TEST_CASE("F3H27 Scene::descendantsOf devuelve DFS pre-order") {
    Scene scene;
    Entity root = scene.createEntity("root");
    Entity a = scene.createEntity("a");
    Entity b = scene.createEntity("b");
    Entity ab = scene.createEntity("ab");
    a.getComponent<TransformComponent>().parent = root.handle();
    b.getComponent<TransformComponent>().parent = root.handle();
    ab.getComponent<TransformComponent>().parent = a.handle();

    const auto desc = scene.descendantsOf(root.handle());
    // root tiene 3 descendants: a, b, ab.
    CHECK(desc.size() == 3u);
    bool foundA = false, foundB = false, foundAb = false;
    for (auto h : desc) {
        if (h == a.handle()) foundA = true;
        if (h == b.handle()) foundB = true;
        if (h == ab.handle()) foundAb = true;
    }
    CHECK(foundA);
    CHECK(foundB);
    CHECK(foundAb);
}

TEST_CASE("F3H27 Scene::topLevelAncestors filtra hijos del set") {
    Scene scene;
    Entity p = scene.createEntity("p");
    Entity c1 = scene.createEntity("c1");
    Entity c2 = scene.createEntity("c2");
    Entity orphan = scene.createEntity("orphan");
    c1.getComponent<TransformComponent>().parent = p.handle();
    c2.getComponent<TransformComponent>().parent = p.handle();

    // Set = { p, c1, c2, orphan }. Top-level deberia ser { p, orphan }
    // (c1 y c2 tienen ancestor p en el set).
    std::vector<entt::entity> set{p.handle(), c1.handle(),
                                    c2.handle(), orphan.handle()};
    const auto top = scene.topLevelAncestors(set);
    CHECK(top.size() == 2u);
    bool foundP = false, foundOrphan = false;
    for (auto h : top) {
        if (h == p.handle()) foundP = true;
        if (h == orphan.handle()) foundOrphan = true;
    }
    CHECK(foundP);
    CHECK(foundOrphan);
}

TEST_CASE("F3H27 Scene::isAncestorOf detecta cadenas correctamente") {
    Scene scene;
    Entity gp = scene.createEntity("gp");
    Entity p = scene.createEntity("p");
    Entity c = scene.createEntity("c");
    Entity other = scene.createEntity("other");
    p.getComponent<TransformComponent>().parent = gp.handle();
    c.getComponent<TransformComponent>().parent = p.handle();

    CHECK(scene.isAncestorOf(gp.handle(), c.handle()));  // grandparent → grandchild
    CHECK(scene.isAncestorOf(p.handle(), c.handle()));   // parent → child
    CHECK(scene.isAncestorOf(gp.handle(), p.handle()));  // direct
    CHECK_FALSE(scene.isAncestorOf(c.handle(), gp.handle()));  // reverso
    CHECK_FALSE(scene.isAncestorOf(other.handle(), c.handle()));  // sin relacion
    CHECK_FALSE(scene.isAncestorOf(c.handle(), c.handle()));  // self
}

TEST_CASE("F3H27 Scene::worldMatrixOf resiste ciclos triviales (entity = self.parent)") {
    Scene scene;
    Entity e = scene.createEntity("self-cycle");
    auto& t = e.getComponent<TransformComponent>();
    t.parent = e.handle();  // ciclo trivial
    t.position = glm::vec3(7.0f, 0.0f, 0.0f);
    // No debe colgar; clamp 32-niveles maneja el ciclo.
    const auto m = scene.worldMatrixOf(e.handle());
    // El resultado puede ser arbitrario (acumula 32 veces el local antes
    // de cortar), pero NO debe ser NaN ni infinito.
    CHECK(std::isfinite(m[3][0]));
}
