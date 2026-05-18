// Tests del helper groupByBatch (F2H4 Bloque B). Cubren la decision
// "batcheable vs non-batcheable" y la integracion con frustum cull.
// El helper es puro (no depende de GL), asi que los tests viven aca
// con un AssetManager stub minimo (mismo patron que test_particle_system).

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/pipeline/Frustum.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MaterialAsset.h"  // F2H63: BlendMode
#include "engine/render/scene_renderer/RenderBatching.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <algorithm>  // F2H63: std::sort para test del sort back-to-front

using namespace Mood;

namespace {

class StubTextureBatch : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory stubFactoryBatch() {
    return [](const std::string&) { return std::make_unique<StubTextureBatch>(); };
}

// Frustum amplio: camara en el origen mirando -Z, FOV 90°, near 0.1,
// far 1000. Casi cualquier AABB delante de la camara cae dentro.
Frustum bigFrustum() {
    const glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 proj = glm::perspective(
        glm::radians(90.0f), 1.0f, 0.1f, 1000.0f);
    return frustumFromViewProj(proj * view);
}

// Spawnea una entidad con cubo primitivo (mesh 0) en pos delante de
// la camara para que pase el cull por default.
Entity spawnCube(Scene& s, const glm::vec3& pos,
                  std::vector<MaterialAssetId> mats = {}) {
    Entity e = s.createEntity("cube");
    auto& t = e.getComponent<TransformComponent>();
    t.position = pos;
    t.scale = glm::vec3(0.5f);
    e.addComponent<MeshRendererComponent>(0u, std::move(mats));
    return e;
}

} // namespace

TEST_CASE("groupByBatch: scene vacia devuelve resultado vacio") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const auto frustum = bigFrustum();
    const auto r = groupByBatch(s, assets, frustum, glm::vec3(0.0f));
    CHECK(r.batches.empty());
    CHECK(r.nonBatchable.empty());
    CHECK(r.culledCount == 0u);
}

TEST_CASE("groupByBatch: 3 entidades mismo (mesh, material) -> 1 batch con 3 mats") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f));
    spawnCube(s, glm::vec3(2.0f, 0.0f, -5.0f));
    spawnCube(s, glm::vec3(-2.0f, 0.0f, -5.0f));

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.size() == 1u);
    CHECK(r.nonBatchable.empty());

    const BatchKey expectedKey{0u, 0u}; // mesh 0, mat 0 (default)
    REQUIRE(r.batches.count(expectedKey) == 1u);
    CHECK(r.batches.at(expectedKey).models.size() == 3u);
}

TEST_CASE("groupByBatch: 2 entidades con materials distintos -> 2 batches") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    // Material 1 != material 0 (default).
    const MaterialAssetId mat1 = assets.createMaterialFromTexture(0u);
    REQUIRE(mat1 != 0u);

    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f), {0u});   // mat 0
    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f), {0u});   // mat 0
    spawnCube(s, glm::vec3(2.0f, 0.0f, -5.0f), {mat1}); // mat 1
    spawnCube(s, glm::vec3(-2.0f, 0.0f, -5.0f), {mat1});// mat 1

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.size() == 2u);
    CHECK(r.nonBatchable.empty());
    CHECK(r.batches.at(BatchKey{0u, 0u}).models.size() == 2u);
    CHECK(r.batches.at(BatchKey{0u, mat1}).models.size() == 2u);
}

TEST_CASE("groupByBatch: entidad con materiales multiples (>1) cae a non-batchable") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const MaterialAssetId mat1 = assets.createMaterialFromTexture(0u);

    // Entidad con dos slots de material -> non-batchable. Aunque el cubo
    // primitivo tenga 1 submesh, la regla del v1 es estricta: si el
    // vector tiene >= 2 entries (potenciales slots distintos), fallback.
    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f), {0u, mat1});
    // Y otra batcheable normal para confirmar que no afecta.
    spawnCube(s, glm::vec3(2.0f, 0.0f, -5.0f));

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.size() == 1u);
    CHECK(r.nonBatchable.size() == 1u);
    CHECK(r.batches.at(BatchKey{0u, 0u}).models.size() == 1u);
}

TEST_CASE("groupByBatch: entidad con SkeletonComponent NO entra (lo maneja skinned pass)") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    Entity skinned = spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f));
    skinned.addComponent<SkeletonComponent>(SkeletonComponent{});
    spawnCube(s, glm::vec3(2.0f, 0.0f, -5.0f));

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    // Solo la no-skinned debe estar en algun lado.
    CHECK(r.batches.size() == 1u);
    CHECK(r.nonBatchable.empty());
    CHECK(r.batches.at(BatchKey{0u, 0u}).models.size() == 1u);
}

TEST_CASE("groupByBatch: entidad fuera del frustum es culled (no batch ni non-batchable)") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    // Una visible delante.
    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f));
    // Una detras de la camara (z=+10) con frustum mirando -Z.
    spawnCube(s, glm::vec3(0.0f, 0.0f, 10.0f));

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.size() == 1u);
    CHECK(r.batches.at(BatchKey{0u, 0u}).models.size() == 1u);
    CHECK(r.nonBatchable.empty());
    CHECK(r.culledCount == 1u);
}

TEST_CASE("groupByBatch: matriz model preserva la posicion del Transform") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    // Posicion dentro del frustum (FOV 90° a z=-10 da plano XY +/-10).
    spawnCube(s, glm::vec3(2.0f, 1.0f, -10.0f));

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    REQUIRE(r.batches.size() == 1u);
    REQUIRE(r.batches.at(BatchKey{0u, 0u}).models.size() == 1u);

    // Translacion de la matriz model = ultima columna (col-major glm).
    const glm::mat4& m = r.batches.at(BatchKey{0u, 0u}).models[0];
    CHECK(m[3].x == doctest::Approx(2.0f).epsilon(0.001));
    CHECK(m[3].y == doctest::Approx(1.0f).epsilon(0.001));
    CHECK(m[3].z == doctest::Approx(-10.0f).epsilon(0.001));
}

TEST_CASE("groupByBatch: stress test 100 cubos visibles -> 1 batch con 100 mats") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    for (int i = 0; i < 100; ++i) {
        spawnCube(s, glm::vec3(static_cast<f32>(i % 10),
                                  static_cast<f32>(i / 10), -10.0f));
    }
    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.size() == 1u);
    CHECK(r.batches.at(BatchKey{0u, 0u}).models.size() == 100u);
}

TEST_CASE("BatchKey: igualdad y hash") {
    BatchKey a{1u, 2u, 0u};
    BatchKey b{1u, 2u, 0u};
    BatchKey c{1u, 3u, 0u};
    CHECK(a == b);
    CHECK_FALSE(a == c);
    BatchKeyHash h;
    CHECK(h(a) == h(b));
    // No es invariante absoluto pero hashes distintos para keys
    // diferentes son lo esperado en este rango.
    CHECK(h(a) != h(c));
}

// ============================================================
// F2H63: bucket translucents (materiales BlendMode != Opaque)
// ============================================================

namespace {

// Crea un material con BlendMode dado (mutando el default opaco).
MaterialAssetId makeBlendMaterial(AssetManager& assets, BlendMode mode,
                                  f32 opacity = 0.5f) {
    const MaterialAssetId mid = assets.createMaterialFromTexture(0u);
    MaterialAsset* m = assets.getMaterial(mid);
    REQUIRE(m != nullptr);
    m->blendMode = mode;
    m->opacity   = opacity;
    return mid;
}

} // namespace

TEST_CASE("groupByBatch F2H63: material Translucent va a translucents (no a batches)") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const MaterialAssetId trans = makeBlendMaterial(assets, BlendMode::Translucent);

    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f), {trans});

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.empty());
    CHECK(r.nonBatchable.empty());
    REQUIRE(r.translucents.size() == 1u);
    CHECK(r.translucents[0].entity);  // valido
}

TEST_CASE("groupByBatch F2H63: BlendMode::Additive tambien rutea a translucents") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const MaterialAssetId add = makeBlendMaterial(assets, BlendMode::Additive);

    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f), {add});

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.empty());
    REQUIRE(r.translucents.size() == 1u);
}

TEST_CASE("groupByBatch F2H63: opacos y translucents se separan en buckets distintos") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const MaterialAssetId trans = makeBlendMaterial(assets, BlendMode::Translucent);

    // 2 opacos + 1 translucent
    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f));            // opaco (mat default)
    spawnCube(s, glm::vec3(2.0f, 0.0f, -5.0f));            // opaco
    spawnCube(s, glm::vec3(-2.0f, 0.0f, -5.0f), {trans});  // translucent

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.batches.size() == 1u);
    REQUIRE(r.batches.count(BatchKey{0u, 0u}) == 1u);
    CHECK(r.batches.at(BatchKey{0u, 0u}).models.size() == 2u);
    CHECK(r.translucents.size() == 1u);
    CHECK(r.nonBatchable.empty());
}

TEST_CASE("groupByBatch F2H63: TranslucentDraw.worldCenter refleja la posicion world-space") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const MaterialAssetId trans = makeBlendMaterial(assets, BlendMode::Translucent);

    // Pos especifica para verificar precision.
    spawnCube(s, glm::vec3(3.0f, 2.0f, -10.0f), {trans});

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    REQUIRE(r.translucents.size() == 1u);
    // El AABB del cubo primitivo es centrado en origen; con scale=0.5
    // el centro world-space coincide con la posicion del transform.
    CHECK(r.translucents[0].worldCenter.x == doctest::Approx(3.0f).epsilon(0.001));
    CHECK(r.translucents[0].worldCenter.y == doctest::Approx(2.0f).epsilon(0.001));
    CHECK(r.translucents[0].worldCenter.z == doctest::Approx(-10.0f).epsilon(0.001));
}

TEST_CASE("groupByBatch F2H63: translucent fuera del frustum es culled (no entra a translucents)") {
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const MaterialAssetId trans = makeBlendMaterial(assets, BlendMode::Translucent);

    // Visible delante.
    spawnCube(s, glm::vec3(0.0f, 0.0f, -5.0f), {trans});
    // Detras de la camara — cull.
    spawnCube(s, glm::vec3(0.0f, 0.0f, 10.0f), {trans});

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    CHECK(r.translucents.size() == 1u);
    CHECK(r.culledCount == 1u);
}

TEST_CASE("groupByBatch F2H63: sort back-to-front por distancia^2 (validacion del comparator)") {
    // No invocamos al sort del renderer aca (no es pure), pero
    // verificamos que worldCenter permite un sort correcto -- el dato
    // que el SceneRenderer usa.
    Scene s;
    AssetManager assets("assets", stubFactoryBatch());
    const MaterialAssetId trans = makeBlendMaterial(assets, BlendMode::Translucent);

    // 3 entidades a distintas distancias de la camara en origen.
    spawnCube(s, glm::vec3(0.0f, 0.0f,  -3.0f), {trans});  // cerca
    spawnCube(s, glm::vec3(0.0f, 0.0f, -10.0f), {trans});  // lejos
    spawnCube(s, glm::vec3(0.0f, 0.0f,  -6.0f), {trans});  // medio

    const auto r = groupByBatch(s, assets, bigFrustum(), glm::vec3(0.0f));
    REQUIRE(r.translucents.size() == 3u);

    // Aplicar el sort comparator que usa el SceneRenderer.
    auto sorted = r.translucents;
    const glm::vec3 cam(0.0f);
    std::sort(sorted.begin(), sorted.end(),
              [&](const TranslucentDraw& a, const TranslucentDraw& b) {
                  const f32 dA = glm::dot(a.worldCenter - cam, a.worldCenter - cam);
                  const f32 dB = glm::dot(b.worldCenter - cam, b.worldCenter - cam);
                  return dA > dB;
              });
    // Orden esperado farthest-first: -10, -6, -3.
    CHECK(sorted[0].worldCenter.z == doctest::Approx(-10.0f).epsilon(0.001));
    CHECK(sorted[1].worldCenter.z == doctest::Approx(-6.0f).epsilon(0.001));
    CHECK(sorted[2].worldCenter.z == doctest::Approx(-3.0f).epsilon(0.001));
}

TEST_CASE("BatchKey: lod distinto produce keys distintas") {
    // F2H6: dos entidades con mismo mesh + material pero distinto LOD
    // deben caer en batches separados (un draw call por LOD).
    BatchKey lod0{1u, 2u, 0u};
    BatchKey lod1{1u, 2u, 1u};
    BatchKey lod2{1u, 2u, 2u};
    CHECK_FALSE(lod0 == lod1);
    CHECK_FALSE(lod1 == lod2);
    CHECK_FALSE(lod0 == lod2);
    BatchKeyHash h;
    // Idealmente hashes distintos en este rango; basta con verificar
    // que la igualdad de keys los distingue.
    CHECK(h(lod0) != h(lod1));
    CHECK(h(lod1) != h(lod2));
}
