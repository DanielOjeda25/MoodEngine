// Tests de la fachada Scene + Entity (Hito 7 Bloque 2).
// Verifican el ciclo de vida basico (create/destroy), componentes, y que la
// fachada no filtra EnTT cuando no es necesario.

#include <doctest/doctest.h>

#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"

#include <glm/vec3.hpp>

#include <string>

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
    a.addComponent<MeshRendererComponent>(nullptr, 1u);
    Entity b = scene.createEntity("b");
    b.addComponent<MeshRendererComponent>(nullptr, 2u);
    Entity c = scene.createEntity("c"); // sin MeshRenderer

    int visited = 0;
    TextureAssetId sumIds = 0;
    scene.forEach<MeshRendererComponent>(
        [&](Entity, MeshRendererComponent& mr) {
            ++visited;
            sumIds += mr.texture;
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
