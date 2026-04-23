// Tests de AssetManager: verifica cache, fallback automatico a missing, y
// respeto al sandbox del VFS. Usa una factoria mock para no depender del
// contexto GL.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"

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
