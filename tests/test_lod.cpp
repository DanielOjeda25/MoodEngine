// Tests del helper selectLod + LodCache (F2H6 Bloque G).
//   - selectLod: matematica pura del threshold por distancia.
//   - submeshesForLod: fallback a LOD 0 cuando el nivel pedido esta vacio.
//   - LodCache::hashLogicalPath: estable y distintivo.
//   - LodCache::tryLoad / save: round-trip + invalidacion por mtime.

#include <doctest/doctest.h>

#include "engine/assets/cache/LodCache.h"
#include "engine/render/resources/MeshAsset.h"
#include "engine/render/rhi/IMesh.h"  // SubMesh::mesh es unique_ptr<IMesh>

#include <glm/vec2.hpp>

#include <filesystem>

using namespace Mood;

TEST_CASE("selectLod: distancia 0 -> LOD 0") {
    const glm::vec2 thresholds(30.0f, 80.0f);
    CHECK(selectLod(0.0f, thresholds) == 0u);
    CHECK(selectLod(15.0f, thresholds) == 0u);
    CHECK(selectLod(29.999f, thresholds) == 0u);
}

TEST_CASE("selectLod: en threshold inferior cae a LOD 1") {
    const glm::vec2 thresholds(30.0f, 80.0f);
    CHECK(selectLod(30.0f, thresholds) == 1u);
    CHECK(selectLod(50.0f, thresholds) == 1u);
    CHECK(selectLod(79.999f, thresholds) == 1u);
}

TEST_CASE("selectLod: en threshold superior cae a LOD 2") {
    const glm::vec2 thresholds(30.0f, 80.0f);
    CHECK(selectLod(80.0f, thresholds) == 2u);
    CHECK(selectLod(150.0f, thresholds) == 2u);
    CHECK(selectLod(1000.0f, thresholds) == 2u);
}

TEST_CASE("selectLod: thresholds custom funcionan igual") {
    const glm::vec2 closeThresholds(5.0f, 15.0f);
    CHECK(selectLod(2.0f,  closeThresholds) == 0u);
    CHECK(selectLod(10.0f, closeThresholds) == 1u);
    CHECK(selectLod(20.0f, closeThresholds) == 2u);
}

TEST_CASE("MeshAsset::submeshesForLod: fallback a LOD 0 si lod1/2 vacios") {
    MeshAsset asset;
    asset.submeshes.emplace_back();  // 1 submesh LOD 0 dummy
    // lod1Submeshes y lod2Submeshes vacios.

    const auto& l0 = asset.submeshesForLod(0);
    const auto& l1 = asset.submeshesForLod(1);
    const auto& l2 = asset.submeshesForLod(2);

    CHECK(l0.size() == 1u);
    // Fallback: ambas listas LOD vacias devuelven LOD 0.
    CHECK(&l1 == &l0);
    CHECK(&l2 == &l0);
}

TEST_CASE("MeshAsset::submeshesForLod: usa lod1Submeshes si esta poblado") {
    MeshAsset asset;
    asset.submeshes.emplace_back();
    asset.lod1Submeshes.emplace_back();
    asset.lod1Submeshes.emplace_back(); // 2 submeshes LOD 1

    const auto& l1 = asset.submeshesForLod(1);
    CHECK(l1.size() == 2u);
    CHECK(&l1 != &asset.submeshes);
}

TEST_CASE("LodCache::hashLogicalPath: estable y distintivo") {
    const u64 a = LodCache::hashLogicalPath("meshes/Fox.glb");
    const u64 b = LodCache::hashLogicalPath("meshes/Fox.glb");
    const u64 c = LodCache::hashLogicalPath("meshes/CesiumMan.glb");
    CHECK(a == b);                  // mismo input -> mismo hash
    CHECK(a != c);                  // distintos inputs -> distintos hashes
    CHECK(a != 0);
}

TEST_CASE("LodCache: round-trip save + tryLoad preserva los datos") {
    const auto cachePath =
        std::filesystem::temp_directory_path() / "moodengine_test_lodcache.moodlod";
    std::filesystem::remove(cachePath);

    LodCache::LodCacheEntry input;
    LodCache::LodSubmeshData sm1;
    sm1.materialIndex = 7u;
    // 19 floats por vertex; metemos 2 vertices (38 floats).
    for (int i = 0; i < 38; ++i) sm1.vertices.push_back(static_cast<f32>(i));
    sm1.vertexCount = 2u;
    input.lod1.push_back(std::move(sm1));

    LodCache::LodSubmeshData sm2;
    sm2.materialIndex = 3u;
    sm2.vertexCount = 0u; // submesh sin LOD generado (fallback)
    input.lod2.push_back(std::move(sm2));

    constexpr u64 kMtime = 1234567890ull;
    constexpr u64 kSize  = 4096ull;
    LodCache::save(cachePath, kMtime, kSize, input);

    LodCache::LodCacheEntry loaded;
    REQUIRE(LodCache::tryLoad(cachePath, kMtime, kSize, loaded));

    REQUIRE(loaded.lod1.size() == 1u);
    CHECK(loaded.lod1[0].materialIndex == 7u);
    CHECK(loaded.lod1[0].vertexCount == 2u);
    REQUIRE(loaded.lod1[0].vertices.size() == 38u);
    for (int i = 0; i < 38; ++i) {
        CHECK(loaded.lod1[0].vertices[i] == doctest::Approx(static_cast<f32>(i)));
    }
    REQUIRE(loaded.lod2.size() == 1u);
    CHECK(loaded.lod2[0].materialIndex == 3u);
    CHECK(loaded.lod2[0].vertexCount == 0u);
    CHECK(loaded.lod2[0].vertices.empty());

    std::filesystem::remove(cachePath);
}

TEST_CASE("LodCache::tryLoad: mtime distinto invalida (cache miss)") {
    const auto cachePath =
        std::filesystem::temp_directory_path() / "moodengine_test_lodcache_mtime.moodlod";
    std::filesystem::remove(cachePath);

    LodCache::LodCacheEntry input;
    LodCache::save(cachePath, /*mtime=*/100ull, /*size=*/4096ull, input);

    LodCache::LodCacheEntry loaded;
    // mtime distinto al guardado -> miss.
    CHECK_FALSE(LodCache::tryLoad(cachePath, /*mtime=*/200ull, /*size=*/4096ull, loaded));

    std::filesystem::remove(cachePath);
}

TEST_CASE("LodCache::tryLoad: size distinto invalida (cache miss)") {
    const auto cachePath =
        std::filesystem::temp_directory_path() / "moodengine_test_lodcache_size.moodlod";
    std::filesystem::remove(cachePath);

    LodCache::LodCacheEntry input;
    LodCache::save(cachePath, /*mtime=*/100ull, /*size=*/4096ull, input);

    LodCache::LodCacheEntry loaded;
    // size distinto al guardado -> miss.
    CHECK_FALSE(LodCache::tryLoad(cachePath, /*mtime=*/100ull, /*size=*/9999ull, loaded));

    std::filesystem::remove(cachePath);
}

TEST_CASE("LodCache::tryLoad: archivo inexistente devuelve false") {
    const auto cachePath =
        std::filesystem::temp_directory_path() / "moodengine_no_existe.moodlod";
    std::filesystem::remove(cachePath); // por si acaso

    LodCache::LodCacheEntry loaded;
    CHECK_FALSE(LodCache::tryLoad(cachePath, 100ull, 4096ull, loaded));
}

TEST_CASE("LodCache::pathFor: produce un path bajo assets/.cache/lods/") {
    const auto p = LodCache::pathFor("meshes/Fox.glb");
    const auto str = p.generic_string();
    CHECK(str.find("assets/.cache/lods/") != std::string::npos);
    CHECK(str.size() > std::string("assets/.cache/lods/").size());
    CHECK(p.extension().string() == ".moodlod");
}
