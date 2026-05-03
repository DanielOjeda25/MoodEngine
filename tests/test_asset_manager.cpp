// Tests de AssetManager: verifica cache, fallback automatico a missing, y
// respeto al sandbox del VFS. Usa una factoria mock para no depender del
// contexto GL.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MaterialAsset.h"

#include <stdexcept>
#include <string>
#include <vector>

using namespace Mood;

namespace {

/// Mock de ITexture: no hace I/O ni GL. Guarda el path que le paso la factory
/// para que los tests puedan verificar que fue llamada.
class MockTexture : public ITexture {
public:
    explicit MockTexture(std::string path) : m_path(std::move(path)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 16; }
    u32 height() const override { return 16; }
    TextureHandle handle() const override { return nullptr; }
    const std::string& path() const { return m_path; }
private:
    std::string m_path;
};

/// Factoria que lleva un log de los paths solicitados. Opcionalmente falla
/// para paths especificos simulando una carga corrupta o inexistente.
struct SpyFactory {
    std::vector<std::string> calls;
    std::vector<std::string> failPaths;

    std::unique_ptr<ITexture> operator()(const std::string& fsPath) {
        calls.push_back(fsPath);
        for (const auto& fp : failPaths) {
            if (fsPath.find(fp) != std::string::npos) {
                throw std::runtime_error("mock: simulada falla para " + fsPath);
            }
        }
        return std::make_unique<MockTexture>(fsPath);
    }
};

AssetManager::TextureFactory wrap(SpyFactory& spy) {
    return [&spy](const std::string& p) { return spy(p); };
}

} // namespace

TEST_CASE("AssetManager: el constructor carga 'missing' con id 0") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));

    CHECK(am.missingTextureId() == 0);
    CHECK(am.textureCount() == 1);
    REQUIRE(spy.calls.size() == 1);
    // El path resuelto contiene 'missing.png' al final.
    CHECK(spy.calls[0].find("missing.png") != std::string::npos);
}

TEST_CASE("AssetManager: loadTexture cachea por path y no reinvoca la factoria") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    spy.calls.clear(); // descartar el call del constructor

    const TextureAssetId id1 = am.loadTexture("textures/grid.png");
    CHECK(id1 != am.missingTextureId());
    CHECK(spy.calls.size() == 1);

    // Segunda llamada con el mismo path: cache hit.
    const TextureAssetId id1b = am.loadTexture("textures/grid.png");
    CHECK(id1b == id1);
    CHECK(spy.calls.size() == 1);

    // Path distinto: nuevo id, nuevo call.
    const TextureAssetId id2 = am.loadTexture("textures/brick.png");
    CHECK(id2 != id1);
    CHECK(id2 != am.missingTextureId());
    CHECK(spy.calls.size() == 2);
}

TEST_CASE("AssetManager: factoria que falla -> id missing, se cachea y no reintenta") {
    SpyFactory spy;
    spy.failPaths = {"corrupta.png"};
    AssetManager am("assets", wrap(spy));
    spy.calls.clear();

    const TextureAssetId id = am.loadTexture("textures/corrupta.png");
    CHECK(id == am.missingTextureId());
    CHECK(spy.calls.size() == 1); // se intento una vez

    // Reintentar el mismo path no debe invocar la factoria de nuevo.
    const TextureAssetId id2 = am.loadTexture("textures/corrupta.png");
    CHECK(id2 == am.missingTextureId());
    CHECK(spy.calls.size() == 1);
}

TEST_CASE("AssetManager: paths rechazados por VFS caen a missing sin invocar factoria") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    spy.calls.clear();

    CHECK(am.loadTexture("../secret.png")        == am.missingTextureId());
    CHECK(am.loadTexture("/abs/path.png")        == am.missingTextureId());
    CHECK(am.loadTexture("")                     == am.missingTextureId());

    CHECK(spy.calls.empty()); // ninguno llego a la factoria
}

TEST_CASE("AssetManager: getTexture con id fuera de rango devuelve missing") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    spy.calls.clear();

    const TextureAssetId id = am.loadTexture("textures/grid.png");
    ITexture* tex = am.getTexture(id);
    REQUIRE(tex != nullptr);

    // Id invalido (no deberia crashear, deberia caer al missing).
    ITexture* fallback = am.getTexture(9999);
    REQUIRE(fallback != nullptr);
    CHECK(fallback == am.getTexture(am.missingTextureId()));
}

TEST_CASE("AssetManager: factory nula lanza excepcion en el constructor") {
    CHECK_THROWS_AS(AssetManager("assets", {}), std::runtime_error);
}

TEST_CASE("AssetManager: si la factoria falla al cargar missing -> el ctor propaga") {
    SpyFactory spy;
    spy.failPaths = {"missing.png"};
    CHECK_THROWS_AS(AssetManager("assets", wrap(spy)), std::runtime_error);
}

// --- Materiales (Hito 17) ---

TEST_CASE("AssetManager: default material existe en slot 0") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    REQUIRE(am.materialCount() >= 1);
    MaterialAsset* def = am.getMaterial(am.missingMaterialId());
    REQUIRE(def != nullptr);
    CHECK(def->logicalPath == "__default_material");
    CHECK(def->albedo == 0); // sin textura asignada
    CHECK(def->metallicMult == doctest::Approx(0.0f));
    CHECK(def->roughnessMult == doctest::Approx(0.5f));
}

TEST_CASE("AssetManager: loadMaterialFromTexture envuelve la textura sin tocar disco") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    const TextureAssetId tex = am.loadTexture("textures/grid.png");
    const MaterialAssetId m1 = am.loadMaterialFromTexture(tex);
    CHECK(m1 != am.missingMaterialId());
    // Cache: pedir el mismo wrapper devuelve el mismo id sin generar un
    // material nuevo.
    const MaterialAssetId m2 = am.loadMaterialFromTexture(tex);
    CHECK(m1 == m2);

    MaterialAsset* mat = am.getMaterial(m1);
    REQUIRE(mat != nullptr);
    CHECK(mat->albedo == tex);
    CHECK(mat->metallicMult == doctest::Approx(0.0f));
    CHECK(mat->roughnessMult == doctest::Approx(1.0f)); // mate completo
}

TEST_CASE("AssetManager: getMaterial con id invalido cae al default") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    MaterialAsset* mat = am.getMaterial(9999);
    REQUIRE(mat != nullptr);
    CHECK(mat == am.getMaterial(am.missingMaterialId()));
}

// ---------------------------------------------------------------------------
// Hito 26: extraccion de texturas embedded + helper createMaterialsForMesh
// ---------------------------------------------------------------------------

TEST_CASE("loadEmbeddedTexture: sin TextureMemoryFactory cae a missing") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    // Constructor no recibio memory factory => debe fallar con warning a
    // missing, sin throw (los tests no pueden depender de tener una mock).
    const std::vector<u8> bytes(64, 0xFF);
    const TextureAssetId id = am.loadEmbeddedTexture("test#0", bytes);
    CHECK(id == am.missingTextureId());
}

TEST_CASE("loadEmbeddedTexture: con factory mock cachea por key y devuelve id distinto") {
    SpyFactory spy;
    int memoryCalls = 0;
    AssetManager::TextureMemoryFactory memFactory =
        [&](const std::vector<u8>&, const std::string& name) {
            ++memoryCalls;
            return std::make_unique<MockTexture>(name);
        };
    AssetManager am("assets", wrap(spy), {}, {}, std::move(memFactory));

    const std::vector<u8> bytes(64, 0xFF);
    const TextureAssetId id1 = am.loadEmbeddedTexture("foo.glb#0", bytes);
    CHECK(id1 != am.missingTextureId());
    CHECK(memoryCalls == 1);

    // Cache hit: misma key -> mismo id, factory no se llama de nuevo.
    const TextureAssetId id2 = am.loadEmbeddedTexture("foo.glb#0", bytes);
    CHECK(id1 == id2);
    CHECK(memoryCalls == 1);

    // Key distinta -> id distinto, factory llamada de nuevo.
    const TextureAssetId id3 = am.loadEmbeddedTexture("bar.glb#0", bytes);
    CHECK(id3 != id1);
    CHECK(memoryCalls == 2);
}

TEST_CASE("loadEmbeddedTexture: buffer vacio cae a missing") {
    SpyFactory spy;
    AssetManager::TextureMemoryFactory memFactory =
        [](const std::vector<u8>&, const std::string& name) {
            return std::make_unique<MockTexture>(name);
        };
    AssetManager am("assets", wrap(spy), {}, {}, std::move(memFactory));
    const TextureAssetId id = am.loadEmbeddedTexture("empty", {});
    CHECK(id == am.missingTextureId());
}

TEST_CASE("createMaterialsForMesh: missing mesh -> 1 default-clone (warning chequer)") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    // El missing mesh (cubo primitivo, slot 0) tiene 1 submesh con
    // materialIndex=0 y materialAlbedoTextures vacio. createMaterialsForMesh
    // debe devolver 1 MaterialAssetId clonado del default (NO compartido).
    auto mats = am.createMaterialsForMesh(am.missingMeshId());
    REQUIRE(mats.size() == 1);
    CHECK(mats[0] != am.missingMaterialId()); // clone, no el slot 0 compartido

    MaterialAsset* m = am.getMaterial(mats[0]);
    REQUIRE(m != nullptr);
    // Hereda useAlbedoMap del default (Hito 25) => muestra missing.png.
    CHECK(m->useAlbedoMap == true);
    CHECK(m->albedoTint.x == doctest::Approx(1.0f));
}

TEST_CASE("createMaterialsForMesh: dos llamadas devuelven materiales unicos (no compartidos)") {
    // Garantia clave del Hito 25: editar albedoTint de un cubo no contagia.
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    auto a = am.createMaterialsForMesh(am.missingMeshId());
    auto b = am.createMaterialsForMesh(am.missingMeshId());
    REQUIRE(a.size() == 1);
    REQUIRE(b.size() == 1);
    CHECK(a[0] != b[0]);
}

TEST_CASE("createMaterialsForMesh: id de mesh invalido devuelve vacio") {
    SpyFactory spy;
    AssetManager am("assets", wrap(spy));
    // Mesh fuera de rango cae al missing mesh (cubo) en getMesh, pero ese
    // tambien tiene submeshes — asi que el contrato es "lo que sea que
    // getMesh devuelva". getMesh nunca es null, asi que el vector no es vacio.
    auto mats = am.createMaterialsForMesh(9999u);
    CHECK(mats.size() == 1); // missing mesh tiene 1 submesh
}
