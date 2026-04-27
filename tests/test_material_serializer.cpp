// Tests de carga de `.material` JSON desde el AssetManager (Hito 17
// Bloque 5). Cubrimos:
//   - Round-trip: archivo escrito a disco -> loadMaterial -> los campos
//     llegan al MaterialAsset.
//   - Defaults: campos faltantes en el JSON caen al default sensato.
//   - Cache: cargas repetidas devuelven el mismo id.
//   - Fallo: archivo inexistente o JSON corrupto -> missingMaterialId().
//   - Texturas referenciadas: el material loader llama a loadTexture
//     internamente; los ids quedan resueltos.
//   - createMaterial: helper runtime devuelve slots distintos siempre.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"
#include "engine/render/MaterialAsset.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace Mood;

namespace {

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

AssetManager::TextureFactory nullFactory() {
    return [](const std::string& p) { return std::make_unique<NullTex>(p); };
}

/// Escribe un .material en `assets/materials/<filename>` (la VFS root del
/// AssetManager es "assets" por convencion de tests). Devuelve el path
/// logico para pasar a `loadMaterial`.
std::string writeMaterial(const std::string& filename, const std::string& json) {
    const auto dir = std::filesystem::path("assets") / "materials";
    std::filesystem::create_directories(dir);
    const auto fs = dir / filename;
    {
        std::ofstream out(fs);
        out << json;
    }
    return std::string("materials/") + filename;
}

std::string uniqueName(const char* base) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::string("mood_test_") + base + "_" +
           std::to_string(stamp) + ".material";
}

} // namespace

TEST_CASE("loadMaterial: round-trip de campos completos") {
    const std::string name = uniqueName("complete");
    const std::string logical = writeMaterial(name, R"({
        "albedo_tint": [0.20, 0.45, 0.85],
        "metallic": 0.75,
        "roughness": 0.30,
        "ao": 0.85
    })");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial(logical);
    REQUIRE(id != am.missingMaterialId());

    MaterialAsset* mat = am.getMaterial(id);
    REQUIRE(mat != nullptr);
    CHECK(mat->albedoTint.x    == doctest::Approx(0.20f));
    CHECK(mat->albedoTint.y    == doctest::Approx(0.45f));
    CHECK(mat->albedoTint.z    == doctest::Approx(0.85f));
    CHECK(mat->metallicMult    == doctest::Approx(0.75f));
    CHECK(mat->roughnessMult   == doctest::Approx(0.30f));
    CHECK(mat->aoMult          == doctest::Approx(0.85f));
    CHECK(mat->logicalPath     == logical);

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("loadMaterial: campos faltantes caen al default") {
    // JSON con SOLO metallic; el resto debe quedar en su default
    // (albedoTint=1, roughness=0.5, ao=1.0).
    const std::string name = uniqueName("partial");
    const std::string logical = writeMaterial(name, R"({
        "metallic": 0.9
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    CHECK(mat->metallicMult  == doctest::Approx(0.9f));
    CHECK(mat->roughnessMult == doctest::Approx(0.5f)); // default
    CHECK(mat->aoMult        == doctest::Approx(1.0f));
    CHECK(mat->albedoTint.x  == doctest::Approx(1.0f));
    CHECK(mat->albedoTint.y  == doctest::Approx(1.0f));
    CHECK(mat->albedoTint.z  == doctest::Approx(1.0f));

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("loadMaterial: cache devuelve el mismo id para el mismo path") {
    const std::string name = uniqueName("cache");
    const std::string logical = writeMaterial(name, R"({"metallic": 1.0})");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id1 = am.loadMaterial(logical);
    const MaterialAssetId id2 = am.loadMaterial(logical);
    CHECK(id1 == id2);
    CHECK(id1 != am.missingMaterialId());

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("loadMaterial: archivo inexistente -> missing") {
    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial("materials/no_existe_xyz.material");
    CHECK(id == am.missingMaterialId());
}

TEST_CASE("loadMaterial: JSON invalido -> missing") {
    const std::string name = uniqueName("broken");
    const std::string logical = writeMaterial(name, "{ esto no es JSON valido }");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial(logical);
    CHECK(id == am.missingMaterialId());

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("loadMaterial: paths de textura se resuelven via loadTexture") {
    // Si el .material referencia un albedo, el AssetManager debe
    // cargar la textura y guardar su id en el slot. Aca usamos
    // textures/missing.png que SIEMPRE existe (es el fallback).
    const std::string name = uniqueName("with_albedo");
    const std::string logical = writeMaterial(name, R"({
        "albedo": "textures/missing.png",
        "metallic": 0.5
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    // missing.png es id 0 — el slot albedo debe quedar resuelto a esa
    // textura existente, NO a 0-como-"no asignado". Verificamos que el
    // path coincida con missing.png (la unica garantia portable; el id
    // exacto depende del orden de carga).
    CHECK(am.pathOf(mat->albedo) == "textures/missing.png");

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("createMaterial: cada llamada genera un slot distinto") {
    AssetManager am("assets", nullFactory());

    MaterialAsset proto1{};
    proto1.albedoTint = glm::vec3(1.0f, 0.0f, 0.0f);
    proto1.metallicMult = 1.0f;

    MaterialAsset proto2{};
    proto2.albedoTint = glm::vec3(0.0f, 1.0f, 0.0f);
    proto2.metallicMult = 0.0f;

    const MaterialAssetId id1 = am.createMaterial(proto1);
    const MaterialAssetId id2 = am.createMaterial(proto2);
    CHECK(id1 != id2);

    MaterialAsset* m1 = am.getMaterial(id1);
    MaterialAsset* m2 = am.getMaterial(id2);
    REQUIRE(m1 != nullptr);
    REQUIRE(m2 != nullptr);
    CHECK(m1->albedoTint.x == doctest::Approx(1.0f));
    CHECK(m2->albedoTint.y == doctest::Approx(1.0f));
    CHECK(m1->metallicMult == doctest::Approx(1.0f));
    CHECK(m2->metallicMult == doctest::Approx(0.0f));
}
