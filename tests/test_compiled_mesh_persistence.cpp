// Tests del round-trip save/load de mesh compilada en .moodmap v13
// (F2H26). Verifican:
//   - Editor escribe `compiledMesh` cuando se le pasa el helper.
//   - Mapa v12 (sin compiledMesh) carga sin compiledMesh => fallback.
//   - Round-trip preserva submeshes (count, materialPath, vertices).
//   - SceneLoader con useCompiledMesh=false ignora compiledMesh y crea
//     BrushComponents.
//   - SceneLoader con useCompiledMesh=true Y compiledMesh presente
//     crea UNA entity con CompiledMeshComponent (sin BrushComponents).
//   - SceneLoader con useCompiledMesh=true SIN compiledMesh cae al
//     fallback de spawnear brushes individuales.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/IMesh.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/CompiledMeshComponent.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/csg/Brush.h"
#include "engine/world/grid/GridMap.h"

#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <memory>

using namespace Mood;

namespace {

class NullTexture : public ITexture {
public:
    explicit NullTexture(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
private:
    std::string m_p;
};

class NullMesh : public IMesh {
public:
    void bind() const override {}
    void unbind() const override {}
    u32 vertexCount() const override { return 0; }
};

AssetManager::TextureFactory nullTextureFactory() {
    return [](const std::string& p) { return std::make_unique<NullTexture>(p); };
}

AssetManager::MeshFactory nullMeshFactory() {
    return [](const std::vector<f32>&, const std::vector<VertexAttribute>&) {
        return std::make_unique<NullMesh>();
    };
}

struct TestAssets {
    std::unique_ptr<AssetManager> mgr;
    TestAssets() {
        mgr = std::make_unique<AssetManager>("assets", nullTextureFactory(),
                                              /*audioFactory=*/nullptr,
                                              nullMeshFactory());
    }
};

std::filesystem::path uniqueTempMap() {
    static int counter = 0;
    ++counter;
    auto p = std::filesystem::temp_directory_path() /
             ("mood_compiledmesh_test_" + std::to_string(counter) + ".moodmap");
    std::error_code ec;
    std::filesystem::remove(p, ec);
    return p;
}

void spawnBoxBrush(Scene& scene, const std::string& tag,
                    const glm::vec3& pos = glm::vec3(0.0f),
                    f32 size = 1.0f) {
    Entity e = scene.createEntity(tag);
    auto& t = e.getComponent<TransformComponent>();
    t.position = pos;
    t.scale = glm::vec3(size);

    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    bc.materials = {0};
    e.addComponent<BrushComponent>(std::move(bc));
}

}  // namespace

// ============================================================
// F2H26 — Schema v13 round-trip
// ============================================================

TEST_CASE("F2H26: save SIN compiledMesh => load lo deja como nullopt") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);
    spawnBoxBrush(scene, "Brush_A");

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "test_map", &scene, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->compiledMesh.has_value());

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("F2H26: save CON compiledMesh => load lo restaura como struct no-vacio") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);
    spawnBoxBrush(scene, "Brush_A");

    auto cm = buildSavedCompiledMeshFromScene(scene, *ta.mgr);
    REQUIRE_FALSE(cm.submeshes.empty());

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "test_map", &scene, *ta.mgr, path, &cm);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->compiledMesh.has_value());
    CHECK(loaded->compiledMesh->submeshes.size() == cm.submeshes.size());
    // Verificar que el primer submesh tiene vertices (11 floats por
    // vertex, mismo formato que brushSubmeshToInterleaved).
    REQUIRE_FALSE(loaded->compiledMesh->submeshes.empty());
    const auto& sub = loaded->compiledMesh->submeshes[0];
    CHECK(sub.vertices.size() % 11u == 0u);
    CHECK(sub.vertices.size() > 0u);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("F2H26: round-trip preserva materialPath y count de vertices") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);
    spawnBoxBrush(scene, "Brush_A");

    auto cm = buildSavedCompiledMeshFromScene(scene, *ta.mgr);
    const usize originalVertCount = cm.submeshes[0].vertices.size();
    const std::string originalMaterialPath = cm.submeshes[0].materialPath;

    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "test_map", &scene, *ta.mgr, path, &cm);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded->compiledMesh.has_value());
    REQUIRE_FALSE(loaded->compiledMesh->submeshes.empty());
    CHECK(loaded->compiledMesh->submeshes[0].materialPath == originalMaterialPath);
    CHECK(loaded->compiledMesh->submeshes[0].vertices.size() == originalVertCount);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

// ============================================================
// F2H26 — useCompiledMesh flag en SceneLoader
// ============================================================

TEST_CASE("F2H26: useCompiledMesh=false ignora compiledMesh => crea BrushComponents") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);
    spawnBoxBrush(scene, "Brush_A");
    spawnBoxBrush(scene, "Brush_B", glm::vec3(3.0f, 0.0f, 0.0f));

    auto cm = buildSavedCompiledMeshFromScene(scene, *ta.mgr);
    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "test_map", &scene, *ta.mgr, path, &cm);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());

    // Cargamos en una scene fresh con useCompiledMesh=false (default).
    Scene fresh;
    SceneLoader::applyEntitiesToScene(*loaded, fresh, *ta.mgr,
                                       /*useCompiledMesh=*/false);

    int brushCount = 0;
    fresh.forEach<BrushComponent>([&](Entity, BrushComponent&) { ++brushCount; });
    CHECK(brushCount == 2);

    int compiledCount = 0;
    fresh.forEach<CompiledMeshComponent>([&](Entity, CompiledMeshComponent&) {
        ++compiledCount;
    });
    CHECK(compiledCount == 0);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("F2H26: useCompiledMesh=true CON compiledMesh => 1 CompiledMeshComponent, 0 BrushComponents") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);
    spawnBoxBrush(scene, "Brush_A");
    spawnBoxBrush(scene, "Brush_B", glm::vec3(3.0f, 0.0f, 0.0f));

    auto cm = buildSavedCompiledMeshFromScene(scene, *ta.mgr);
    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "test_map", &scene, *ta.mgr, path, &cm);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->compiledMesh.has_value());

    Scene fresh;
    SceneLoader::applyEntitiesToScene(*loaded, fresh, *ta.mgr,
                                       /*useCompiledMesh=*/true);

    int brushCount = 0;
    fresh.forEach<BrushComponent>([&](Entity, BrushComponent&) { ++brushCount; });
    CHECK(brushCount == 0);  // brushes individuales NO se crean

    int compiledCount = 0;
    fresh.forEach<CompiledMeshComponent>([&](Entity, CompiledMeshComponent&) {
        ++compiledCount;
    });
    CHECK(compiledCount == 1);  // 1 entity unificada

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("F2H26: useCompiledMesh=true SIN compiledMesh => fallback a brushes individuales") {
    TestAssets ta;
    Scene scene;
    GridMap map(4, 4, 1.0f);
    spawnBoxBrush(scene, "Brush_A");
    spawnBoxBrush(scene, "Brush_B", glm::vec3(3.0f, 0.0f, 0.0f));

    // Save SIN compiledMesh (caller no provee el helper).
    const auto path = uniqueTempMap();
    SceneSerializer::save(map, "test_map", &scene, *ta.mgr, path);

    auto loaded = SceneSerializer::load(path, *ta.mgr);
    REQUIRE(loaded.has_value());
    REQUIRE_FALSE(loaded->compiledMesh.has_value());

    Scene fresh;
    SceneLoader::applyEntitiesToScene(*loaded, fresh, *ta.mgr,
                                       /*useCompiledMesh=*/true);

    // Como el savedMap no tiene compiledMesh, useCompiledMesh=true cae
    // al fallback: spawnea los brushes individuales.
    int brushCount = 0;
    fresh.forEach<BrushComponent>([&](Entity, BrushComponent&) { ++brushCount; });
    CHECK(brushCount == 2);

    int compiledCount = 0;
    fresh.forEach<CompiledMeshComponent>([&](Entity, CompiledMeshComponent&) {
        ++compiledCount;
    });
    CHECK(compiledCount == 0);

    std::error_code ec;
    std::filesystem::remove(path, ec);
}

TEST_CASE("F2H26: scene sin brushes => buildSavedCompiledMeshFromScene devuelve struct vacio") {
    TestAssets ta;
    Scene scene;
    auto cm = buildSavedCompiledMeshFromScene(scene, *ta.mgr);
    CHECK(cm.submeshes.empty());
}
