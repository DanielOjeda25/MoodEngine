// Tests del round-trip save/load de brushes CSG en .moodmap (F2H11
// Bloque E). Verifican:
//   - Round-trip de 1 brush preserva planos (normal + distance).
//   - Round-trip preserva transform (position, rotation, scale).
//   - Round-trip preserva tag.
//   - Round-trip preserva materialIndex per-cara.
//   - N brushes con configs distintas se persisten todos.
//   - Mapas viejos sin `brushes[]` se cargan sin error (lista vacia).
//   - El brush re-cargado produce mesh identica al original via
//     buildBrushMesh (validacion semantica end-to-end).
//
// Estos tests usan SceneSerializer + SceneLoader directos, sin
// instanciar AssetManager con dependencias graficas (las funciones
// puras de Brush.cpp / BrushMesh.cpp no requieren GL).

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/csg/Brush.h"
#include "engine/world/csg/BrushMesh.h"
#include "engine/world/grid/GridMap.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>

using namespace Mood;

namespace {

constexpr f32 k_eps = 1e-3f;

class NullTexture : public ITexture {
public:
    explicit NullTexture(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
    const std::string& path() const { return m_p; }
private:
    std::string m_p;
};

AssetManager::TextureFactory nullFactory() {
    return [](const std::string& p) { return std::make_unique<NullTexture>(p); };
}

struct TestAssets {
    std::unique_ptr<AssetManager> mgr;
    TestAssets() {
        // Reusa el assets/ del repo: tiene missing.png y demas
        // fallbacks que el constructor de AssetManager exige.
        mgr = std::make_unique<AssetManager>("assets", nullFactory());
    }
};

std::filesystem::path uniqueTempMap() {
    static int counter = 0;
    ++counter;
    auto p = std::filesystem::temp_directory_path() /
             ("mood_brush_test_" + std::to_string(counter) + ".moodmap");
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}

/// @brief Crea una scene con N box brushes con transforms distintos.
void buildSceneWithBrushes(Scene& scene, int n) {
    for (int i = 0; i < n; ++i) {
        const std::string tag = "Brush_" + std::to_string(i);
        Entity e = scene.createEntity(tag);
        auto& t = e.getComponent<TransformComponent>();
        t.position = glm::vec3(static_cast<f32>(i) * 2.0f,
                                static_cast<f32>(i) + 1.0f,
                                static_cast<f32>(-i));
        t.rotationEuler = glm::vec3(0.0f, static_cast<f32>(i) * 15.0f, 0.0f);
        t.scale = glm::vec3(1.0f);

        BrushComponent bc;
        // Brush local (sin transform aplicado al makeBoxBrush — el
        // transform vive en TransformComponent y se aplica al render).
        bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
        bc.material = 0;
        bc.dirty = true;
        e.addComponent<BrushComponent>(std::move(bc));
    }
}

}  // namespace

// ============================================================
// Round-trip de 1 brush
// ============================================================

TEST_CASE("save+load: 1 box brush preserva 6 caras con normales canonicas") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);

    Entity e = scene.createEntity("Brush_Test");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(5.0f, 2.0f, -3.0f);

    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    bc.material = 0;
    bc.dirty = true;
    e.addComponent<BrushComponent>(std::move(bc));

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "test_map", &scene, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->brushes.size() == 1);

    const auto& sb = loaded->brushes[0];
    CHECK(sb.tag == "Brush_Test");
    CHECK(sb.faces.size() == 6);

    // Las 6 normales canonicas deben aparecer (en algun orden).
    const glm::vec3 expected[6] = {
        { 1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0,-1, 0}, {0, 0, 1}, {0, 0,-1}
    };
    for (const auto& exp : expected) {
        int matches = 0;
        for (const auto& f : sb.faces) {
            if (glm::length(f.normal - exp) < k_eps) ++matches;
        }
        CHECK(matches == 1);
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("save+load: transform (position, rotationEuler, scale) preservado") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);

    Entity e = scene.createEntity("Brush_Xform");
    auto& t = e.getComponent<TransformComponent>();
    t.position      = glm::vec3(7.5f, -2.25f, 0.125f);
    t.rotationEuler = glm::vec3(15.0f, 45.0f, -90.0f);
    t.scale         = glm::vec3(2.0f, 3.0f, 0.5f);

    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    bc.dirty = true;
    e.addComponent<BrushComponent>(std::move(bc));

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "xf", &scene, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->brushes.size() == 1);

    const auto& sb = loaded->brushes[0];
    CHECK(sb.position.x == doctest::Approx(7.5f).epsilon(k_eps));
    CHECK(sb.position.y == doctest::Approx(-2.25f).epsilon(k_eps));
    CHECK(sb.position.z == doctest::Approx(0.125f).epsilon(k_eps));
    CHECK(sb.rotationEuler.y == doctest::Approx(45.0f).epsilon(k_eps));
    CHECK(sb.rotationEuler.z == doctest::Approx(-90.0f).epsilon(k_eps));
    CHECK(sb.scale.x == doctest::Approx(2.0f).epsilon(k_eps));
    CHECK(sb.scale.y == doctest::Approx(3.0f).epsilon(k_eps));
    CHECK(sb.scale.z == doctest::Approx(0.5f).epsilon(k_eps));

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("save+load: planos (normal + distance) bit-similar") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);

    // Brush no-canonico: rotado 30 grados en Y para que las normales
    // no sean los ejes mundiales puros — verifica que el round-trip
    // float preserva las normales con tolerancia kPlaneEpsilon.
    const glm::mat4 R = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f),
                                     glm::vec3(0, 1, 0));
    Entity e = scene.createEntity("Brush_Rot");
    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(R);
    bc.dirty = true;
    const Csg::Brush originalBrush = bc.brush;  // copia para comparar
    e.addComponent<BrushComponent>(std::move(bc));

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "rot", &scene, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->brushes.size() == 1);

    // Para cada cara del brush original, debe existir una cara en el
    // SavedBrush con la MISMA normal y MISMA distance (mod tolerance).
    const auto& sb = loaded->brushes[0];
    CHECK(sb.faces.size() == originalBrush.faces.size());
    for (const auto& origFace : originalBrush.faces) {
        bool found = false;
        for (const auto& sf : sb.faces) {
            const f32 normalDiff = glm::length(sf.normal - origFace.plane.normal);
            const f32 distDiff = std::fabs(sf.distance - origFace.plane.distance);
            if (normalDiff < k_eps && distDiff < k_eps) {
                found = true;
                break;
            }
        }
        CHECK(found);
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ============================================================
// applyEntitiesToScene: reconstruccion del componente
// ============================================================

TEST_CASE("applyEntitiesToScene: reconstruye BrushComponent en el scene") {
    TestAssets ta;
    Scene sceneOriginal;
    GridMap map(4, 4, 1.0f);
    buildSceneWithBrushes(sceneOriginal, 3);

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "multi", &sceneOriginal, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->brushes.size() == 3);

    Scene sceneRestored;
    SceneLoader::applyEntitiesToScene(*loaded, sceneRestored, *ta.mgr);

    int restoredCount = 0;
    sceneRestored.forEach<BrushComponent>(
        [&](Entity, BrushComponent& bc) {
            CHECK(bc.brush.faces.size() == 6);
            CHECK(bc.dirty == true);  // arranca dirty -> primer frame regenera mesh
            ++restoredCount;
        });
    CHECK(restoredCount == 3);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("applyEntitiesToScene: brush re-cargado produce buildBrushMesh con 24 vertices") {
    TestAssets ta;
    Scene sceneOriginal;
    GridMap map(4, 4, 1.0f);

    Entity e = sceneOriginal.createEntity("Brush_Roundtrip");
    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    bc.dirty = true;
    e.addComponent<BrushComponent>(std::move(bc));

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "rt", &sceneOriginal, *ta.mgr, path);
    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());

    Scene sceneRestored;
    SceneLoader::applyEntitiesToScene(*loaded, sceneRestored, *ta.mgr);

    bool found = false;
    sceneRestored.forEach<BrushComponent>(
        [&](Entity, BrushComponent& bc2) {
            const Csg::BrushMeshData mesh = Csg::buildBrushMesh(bc2.brush);
            CHECK(mesh.vertices.size() == 24);
            CHECK(mesh.indices.size() == 36);
            found = true;
        });
    CHECK(found);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ============================================================
// Back-compat
// ============================================================

TEST_CASE("load: mapa v9 sin brushes[] se carga con lista vacia") {
    TestAssets ta;

    // Construir un .moodmap v9 minimo a mano (sin la seccion brushes).
    const auto path = uniqueTempMap();
    {
        std::ofstream out(path);
        out << R"({
            "version": 9,
            "name": "legacy_v9",
            "width": 2,
            "height": 2,
            "tileSize": 1.0,
            "tiles": [
                {"type": "empty", "texture": ""},
                {"type": "empty", "texture": ""},
                {"type": "empty", "texture": ""},
                {"type": "empty", "texture": ""}
            ],
            "entities": []
        })";
    }
    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    CHECK(loaded->brushes.empty());

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("save: brushes y entidades viven en arrays separados (sin doble-persistencia)") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);

    // Una entidad solo con MeshRenderer (-> entities[]).
    Entity e1 = scene.createEntity("Mesh_E1");
    e1.addComponent<MeshRendererComponent>(
        ta.mgr->missingMeshId(), ta.mgr->missingMaterialId());

    // Una entidad solo con BrushComponent (-> brushes[]).
    Entity e2 = scene.createEntity("Brush_E2");
    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    e2.addComponent<BrushComponent>(std::move(bc));

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "split", &scene, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    CHECK(loaded->entities.size() == 1);
    CHECK(loaded->entities[0].tag == "Mesh_E1");
    CHECK(loaded->brushes.size() == 1);
    CHECK(loaded->brushes[0].tag == "Brush_E2");

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("save+load: materialIndex per-cara preservado") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);

    Entity e = scene.createEntity("Brush_Mats");
    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    // Asignar materialIndex distinto a cada cara.
    REQUIRE(bc.brush.faces.size() == 6);
    for (u32 i = 0; i < 6; ++i) {
        bc.brush.faces[i].materialIndex = i + 10;
    }
    e.addComponent<BrushComponent>(std::move(bc));

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "mats", &scene, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->brushes.size() == 1);

    const auto& sb = loaded->brushes[0];
    CHECK(sb.faces.size() == 6);
    // Verificar que los materialIndex estan presentes (en algun orden).
    std::vector<u32> indices;
    for (const auto& f : sb.faces) indices.push_back(f.materialIndex);
    std::sort(indices.begin(), indices.end());
    for (u32 i = 0; i < 6; ++i) {
        CHECK(indices[i] == i + 10);
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
}
