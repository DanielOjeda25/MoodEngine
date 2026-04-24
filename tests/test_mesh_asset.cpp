// Tests de MeshLoader + AssetManager::loadMesh (Hito 10 Bloque 7).
// Escribe un .obj trivial a un path temporal y verifica que assimp lo
// importa con el submesh + vertex count esperados. Usa una MeshFactory
// mock para no requerir contexto GL.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/assets/MeshLoader.h"
#include "engine/render/IMesh.h"
#include "engine/render/ITexture.h"
#include "engine/render/MeshAsset.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace Mood;

namespace {

class StubMesh : public IMesh {
public:
    explicit StubMesh(u32 count) : m_count(count) {}
    void bind() const override {}
    void unbind() const override {}
    u32 vertexCount() const override { return m_count; }
private:
    u32 m_count;
};

class NullTex : public ITexture {
public:
    explicit NullTex(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
    const std::string& path() const { return m_p; }
private:
    std::string m_p;
};

AssetManager::TextureFactory nullTexFactory() {
    return [](const std::string& p) { return std::make_unique<NullTex>(p); };
}

AssetManager::MeshFactory stubMeshFactory() {
    return [](const std::vector<f32>& verts,
              const std::vector<VertexAttribute>& attrs) -> std::unique_ptr<IMesh> {
        u32 stride = 0;
        for (const auto& a : attrs) stride += a.componentCount;
        const u32 count = (stride > 0) ? static_cast<u32>(verts.size() / stride) : 0;
        return std::make_unique<StubMesh>(count);
    };
}

std::filesystem::path writeTempObj(const std::string& content) {
    const auto dir = std::filesystem::temp_directory_path();
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = dir / ("mood_test_mesh_" + std::to_string(stamp) + ".obj");
    std::ofstream f(path);
    f << content;
    return path;
}

} // namespace

TEST_CASE("MeshLoader: OBJ de un triangulo devuelve 1 submesh con 3 vertices") {
    // OBJ minimo: 3 vertices + 1 cara triangular. Sin UVs ni normales;
    // assimp debe procesar aun asi gracias a aiProcess_GenNormals.
    const auto path = writeTempObj(
        "v -1.0 0.0 0.0\n"
        "v  1.0 0.0 0.0\n"
        "v  0.0 1.0 0.0\n"
        "f 1 2 3\n");

    auto asset = loadMeshWithAssimp("meshes/triangle.obj", path.generic_string(),
                                     stubMeshFactory());
    REQUIRE(asset != nullptr);
    CHECK(asset->logicalPath == "meshes/triangle.obj");
    REQUIRE(asset->submeshes.size() == 1);
    CHECK(asset->submeshes[0].vertexCount == 3); // 1 tri * 3 verts
    CHECK(asset->totalVertexCount() == 3);

    std::filesystem::remove(path);
}

TEST_CASE("MeshLoader: archivo inexistente devuelve nullptr sin crashear") {
    const auto fake = std::filesystem::temp_directory_path() / "no_existe_12345.obj";
    std::filesystem::remove(fake);
    auto asset = loadMeshWithAssimp("meshes/no_existe.obj", fake.generic_string(),
                                     stubMeshFactory());
    CHECK(asset == nullptr);
}

TEST_CASE("MeshLoader: archivo corrupto devuelve nullptr") {
    const auto path = writeTempObj("esto no es un OBJ valido @#$ <html>\n");
    auto asset = loadMeshWithAssimp("meshes/corrupto.obj", path.generic_string(),
                                     stubMeshFactory());
    CHECK(asset == nullptr);
    std::filesystem::remove(path);
}

TEST_CASE("AssetManager::loadMesh cachea paths y cae a missing ante fallo") {
    AssetManager am("assets", nullTexFactory(), {}, stubMeshFactory());

    // Slot 0 siempre es el cubo fallback.
    MeshAsset* fallback = am.getMesh(am.missingMeshId());
    REQUIRE(fallback != nullptr);
    CHECK(fallback->logicalPath == "__missing_cube");
    CHECK(fallback->submeshes.size() == 1);

    // Path invalido (../) cae al missing via VFS.
    const MeshAssetId bad = am.loadMesh("../leak.obj");
    CHECK(bad == am.missingMeshId());

    // Id fuera de rango tambien cae al missing.
    MeshAsset* got = am.getMesh(9999);
    REQUIRE(got != nullptr);
    CHECK(got == fallback);
}

TEST_CASE("AssetManager::loadMesh carga pyramid.obj del repo y lo cachea") {
    // Usa el asset real committed en assets/meshes/pyramid.obj. El
    // AssetManager se construye contra la carpeta assets/ del repo (misma
    // convencion que los otros tests), cuyo root contiene missing.png +
    // missing.wav para que el ctor no tire.
    AssetManager am("assets", nullTexFactory(), {}, stubMeshFactory());
    const MeshAssetId id = am.loadMesh("meshes/pyramid.obj");
    CHECK(id != am.missingMeshId());

    MeshAsset* asset = am.getMesh(id);
    REQUIRE(asset != nullptr);
    CHECK(asset->logicalPath == "meshes/pyramid.obj");
    CHECK(asset->submeshes.size() >= 1);
    CHECK(asset->totalVertexCount() > 0);

    // Segunda llamada = cache hit, mismo id.
    CHECK(am.loadMesh("meshes/pyramid.obj") == id);
    CHECK(am.meshPathOf(id) == "meshes/pyramid.obj");
}
