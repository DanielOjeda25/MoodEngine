// F2H75 Bloque B: tests headless del PhysicsWorld SoftBody/Cloth API.
// Verifican lifecycle (create/destroy/count), que las particulas ancladas
// NO se muevan bajo gravedad mientras las libres caen, y que el read de
// vertices devuelva la cantidad correcta en world space.

#include <doctest/doctest.h>

#include "engine/physics/cloth/ClothLayout.h"
#include "engine/physics/world/PhysicsWorld.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>

#include <vector>

using namespace Mood;

namespace {
constexpr f32 k_dt = 0.016f;

// Tela en su orientacion natural (plano XY, vertical) a 5 m de altura.
// Colgando del borde superior ya esta en reposo: util para lifecycle/read.
glm::mat4 clothSpawnTransform() {
    glm::mat4 m(1.0f);
    m[3] = glm::vec4(0.0f, 5.0f, 0.0f, 1.0f);
    return m;
}

// Tela rotada a HORIZONTAL (plano XZ) a 5 m: el plano XY local se acuesta
// rotando -90° sobre X. Asi, anclada por una fila, el resto se hunde por
// gravedad (como un mantel que cuelga de una mesa). Sirve para ver caida.
glm::mat4 clothSpawnHorizontal() {
    glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f));
    m = glm::rotate(m, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    return m;
}
} // namespace

TEST_CASE("PhysicsWorld F2H75: createCloth devuelve handle + clothCount=1") {
    PhysicsWorld pw;
    const auto layout = cloth::buildGridCloth(2.0f, 2.0f, 6, 6,
                                               cloth::AnchorEdge::TopEdge);
    REQUIRE(!layout.empty());
    REQUIRE(pw.clothCount() == 0u);

    const u32 id = pw.createCloth(layout, clothSpawnTransform(),
                                   /*mass*/ 1.0f, /*compliance*/ 0.0f,
                                   /*damping*/ 0.1f, /*gravity*/ true);
    CHECK(id != 0u);
    CHECK(pw.clothCount() == 1u);

    pw.destroyCloth(id);
    CHECK(pw.clothCount() == 0u);
}

TEST_CASE("PhysicsWorld F2H75: readClothVertices devuelve N posiciones") {
    PhysicsWorld pw;
    const auto layout = cloth::buildGridCloth(2.0f, 2.0f, 5, 5,
                                               cloth::AnchorEdge::TopEdge);
    const u32 id = pw.createCloth(layout, clothSpawnTransform(),
                                   1.0f, 0.0f, 0.1f, true);
    REQUIRE(id != 0u);

    std::vector<glm::vec3> verts;
    REQUIRE(pw.readClothVertices(id, verts));
    CHECK(verts.size() == static_cast<usize>(layout.vertexCount()));
    // id invalido -> false.
    std::vector<glm::vec3> dummy;
    CHECK_FALSE(pw.readClothVertices(9999u, dummy));
}

TEST_CASE("PhysicsWorld F2H75: tela cuelga - libres caen, ancladas se quedan") {
    PhysicsWorld pw;
    const int rx = 6, ry = 6;
    const auto layout = cloth::buildGridCloth(2.0f, 2.0f, rx, ry,
                                               cloth::AnchorEdge::TopEdge);
    // Horizontal: anclada por una fila, el resto cuelga y se hunde.
    const u32 id = pw.createCloth(layout, clothSpawnHorizontal(),
                                   1.0f, 0.0f, 0.1f, /*gravity*/ true);
    REQUIRE(id != 0u);

    std::vector<glm::vec3> before;
    REQUIRE(pw.readClothVertices(id, before));

    // Simular ~1s: la tela debe hundirse colgando del borde anclado.
    for (int i = 0; i < 60; ++i) pw.step(k_dt);

    std::vector<glm::vec3> after;
    REQUIRE(pw.readClothVertices(id, after));
    REQUIRE(after.size() == before.size());

    // Fila superior (y=0) anclada: practicamente no se mueve.
    for (int x = 0; x < rx; ++x) {
        const int i = cloth::ClothLayout::idx(x, 0, rx);
        const f32 moved = glm::length(after[i] - before[i]);
        CHECK(moved < 0.05f);
    }
    // Fila inferior (y=ry-1) libre: cae notablemente en -Y.
    f32 bottomDrop = 0.0f;
    for (int x = 0; x < rx; ++x) {
        const int i = cloth::ClothLayout::idx(x, ry - 1, rx);
        bottomDrop += (before[i].y - after[i].y);
    }
    bottomDrop /= static_cast<f32>(rx);
    CHECK(bottomDrop > 0.1f);  // el borde de abajo bajo al menos 10 cm
}

TEST_CASE("PhysicsWorld F2H75: applyClothAcceleration empuja la tela") {
    PhysicsWorld pw;
    const int rx = 6, ry = 6;
    const auto layout = cloth::buildGridCloth(2.0f, 2.0f, rx, ry,
                                               cloth::AnchorEdge::TopEdge);
    // Sin gravedad para aislar el efecto del viento.
    const u32 id = pw.createCloth(layout, clothSpawnTransform(),
                                   1.0f, 0.0f, 0.0f, /*gravity*/ false);
    REQUIRE(id != 0u);

    std::vector<glm::vec3> before;
    REQUIRE(pw.readClothVertices(id, before));

    // Viento fuerte en +Z durante ~0.5s.
    for (int i = 0; i < 30; ++i) {
        pw.applyClothAcceleration(id, glm::vec3(0.0f, 0.0f, 30.0f), k_dt);
        pw.step(k_dt);
    }

    std::vector<glm::vec3> after;
    REQUIRE(pw.readClothVertices(id, after));
    // El borde inferior libre debe haberse desplazado en +Z.
    f32 zShift = 0.0f;
    for (int x = 0; x < rx; ++x) {
        const int i = cloth::ClothLayout::idx(x, ry - 1, rx);
        zShift += (after[i].z - before[i].z);
    }
    zShift /= static_cast<f32>(rx);
    CHECK(zShift > 0.05f);
}
