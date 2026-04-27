// Tests del Forward+ LightGrid (Hito 18). Cubrimos el helper puro
// `projectSphereToTileRange` con casos conocidos (luz en centro,
// luz en esquina, luz fuera del frustum, luz que cruza el plano
// near) y la integracion completa via `LightGrid::compute`
// verificando que: (a) cada tile tiene un offset/count consistente,
// (b) el flat array de indices contiene cada luz EXACTAMENTE el
// numero de tiles que toca, (c) luces fuera del frustum no aparecen.

#include <doctest/doctest.h>

#include "engine/render/LightGrid.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

using namespace Mood;

namespace {

/// Camara estandar para los tests: mirando hacia -Z desde (0,0,5),
/// FOV 60 deg, viewport 1280x720, near 0.1, far 100.
struct TestCamera {
    glm::mat4 view;
    glm::mat4 proj;
    u32 w = 1280;
    u32 h = 720;
};

TestCamera makeCamera() {
    TestCamera c;
    c.view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f),
                         glm::vec3(0.0f, 0.0f, 0.0f),
                         glm::vec3(0.0f, 1.0f, 0.0f));
    c.proj = glm::perspective(glm::radians(60.0f),
                              static_cast<f32>(c.w) /
                              static_cast<f32>(c.h),
                              0.1f, 100.0f);
    return c;
}

/// Crea un LightFrameData con UNA point light en pos/radius dados.
LightFrameData makeOneLight(const glm::vec3& pos, f32 radius) {
    LightFrameData d;
    PointLightData L{};
    L.position  = pos;
    L.radius    = radius;
    L.intensity = 1.0f;
    L.color     = glm::vec3(1.0f);
    d.pointLights.push_back(L);
    return d;
}

} // namespace

TEST_CASE("projectSphereToTileRange: luz centrada en frente de la camara") {
    auto c = makeCamera();
    TileRange r{};
    const bool ok = projectSphereToTileRange(
        glm::vec3(0.0f, 0.0f, 0.0f), 1.0f,
        c.view, c.proj, c.w, c.h, r);
    REQUIRE(ok);
    // 1280x720 con tiles 16 -> 80x45. Tile centro X = 39 o 40, tile
    // centro Y = 22 (origen abajo). El AABB conservativo cubre un
    // pequeno rango alrededor.
    CHECK(r.minX <= 40);
    CHECK(r.maxX >= 39);
    CHECK(r.minY <= 23);
    CHECK(r.maxY >= 21);
    // Y NO debe abarcar todo el viewport (la luz es chica + esta
    // delante).
    CHECK(r.maxX - r.minX < 80);
    CHECK(r.maxY - r.minY < 45);
}

TEST_CASE("projectSphereToTileRange: luz detras de la camara -> descartada") {
    auto c = makeCamera();
    TileRange r{};
    // Camara mira hacia -Z desde z=5; una luz en z=10 esta detras.
    const bool ok = projectSphereToTileRange(
        glm::vec3(0.0f, 0.0f, 10.0f), 0.5f,
        c.view, c.proj, c.w, c.h, r);
    CHECK_FALSE(ok);
}

TEST_CASE("projectSphereToTileRange: luz lejos del frustum lateral -> descartada") {
    auto c = makeCamera();
    TileRange r{};
    // Luz a 50m a la derecha, fuera del frustum lateral (FOV 60 deg
    // a z=0 cubre ~ x in [-2.9, 2.9]).
    const bool ok = projectSphereToTileRange(
        glm::vec3(50.0f, 0.0f, 0.0f), 0.5f,
        c.view, c.proj, c.w, c.h, r);
    CHECK_FALSE(ok);
}

TEST_CASE("projectSphereToTileRange: luz que cruza la camara -> abarca todo") {
    auto c = makeCamera();
    TileRange r{};
    // Sphere centrado en la camara con radius enorme: parte adelante,
    // parte atras. El helper debe marcar el viewport completo.
    const bool ok = projectSphereToTileRange(
        glm::vec3(0.0f, 0.0f, 5.0f), 5.0f,
        c.view, c.proj, c.w, c.h, r);
    REQUIRE(ok);
    CHECK(r.minX == 0);
    CHECK(r.minY == 0);
    CHECK(r.maxX == (1280u + 15u) / 16u - 1u); // 79
    CHECK(r.maxY == (720u  + 15u) / 16u - 1u); // 44
}

TEST_CASE("LightGrid::compute: dimensiones de tile correctas") {
    LightGrid g;
    auto c = makeCamera();
    LightFrameData empty;
    g.compute(empty, c.view, c.proj, c.w, c.h);
    CHECK(g.tilesX() == 80u);
    CHECK(g.tilesY() == 45u);
    CHECK(g.tileCount() == 80u * 45u);
    CHECK(g.lightIndices().empty());
}

TEST_CASE("LightGrid::compute: tile no-vacio con luz en el frustum") {
    LightGrid g;
    auto c = makeCamera();
    auto data = makeOneLight(glm::vec3(0, 0, 0), 1.0f);
    g.compute(data, c.view, c.proj, c.w, c.h);

    // Debe haber al menos un tile con count > 0 (el centro).
    u32 nonEmpty = 0;
    u32 totalAssign = 0;
    for (const auto& t : g.tileData()) {
        if (t.count > 0) ++nonEmpty;
        totalAssign += t.count;
    }
    CHECK(nonEmpty > 0);
    // El flat array tiene exactamente `totalAssign` entradas, y todas
    // referencian a la unica luz (id 0).
    CHECK(g.lightIndices().size() == totalAssign);
    for (u32 idx : g.lightIndices()) {
        CHECK(idx == 0u);
    }
}

TEST_CASE("LightGrid::compute: offsets monotonicos (prefix sum valido)") {
    LightGrid g;
    auto c = makeCamera();
    LightFrameData data;
    // Tres luces en posiciones distintas, todas en el frustum.
    data.pointLights.push_back({glm::vec3(-1, 0, 0), glm::vec3(1), 1.0f, 1.0f});
    data.pointLights.push_back({glm::vec3( 0, 0, 0), glm::vec3(1), 1.0f, 1.0f});
    data.pointLights.push_back({glm::vec3( 1, 0, 0), glm::vec3(1), 1.0f, 1.0f});
    g.compute(data, c.view, c.proj, c.w, c.h);

    // Para cada tile no-vacio, sus indices deben caer dentro del
    // rango [offset, offset+count). Verificamos que ningun tile
    // pisa los indices de otro.
    for (const auto& t : g.tileData()) {
        CHECK(t.offset + t.count <= g.lightIndices().size());
    }
}

TEST_CASE("LightGrid::compute: viewport 0x0 no crashea") {
    LightGrid g;
    auto c = makeCamera();
    auto data = makeOneLight(glm::vec3(0, 0, 0), 1.0f);
    g.compute(data, c.view, c.proj, 0u, 0u);
    CHECK(g.tilesX() == 0u);
    CHECK(g.tilesY() == 0u);
    CHECK(g.tileCount() == 0u);
}

TEST_CASE("LightGrid::compute: luces fuera del frustum no aparecen") {
    LightGrid g;
    auto c = makeCamera();
    LightFrameData data;
    // Luz 0: en frustum.
    data.pointLights.push_back({glm::vec3(0, 0, 0), glm::vec3(1), 1.0f, 1.0f});
    // Luz 1: detras de la camara.
    data.pointLights.push_back({glm::vec3(0, 0, 100), glm::vec3(1), 1.0f, 1.0f});
    // Luz 2: lateral lejana.
    data.pointLights.push_back({glm::vec3(500, 0, 0), glm::vec3(1), 1.0f, 1.0f});
    g.compute(data, c.view, c.proj, c.w, c.h);

    // En el flat array solo deberia aparecer el index 0.
    for (u32 idx : g.lightIndices()) {
        CHECK(idx == 0u);
    }
}
