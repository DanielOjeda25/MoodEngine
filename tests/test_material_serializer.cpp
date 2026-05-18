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

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/render/resources/MaterialAsset.h"

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

TEST_CASE("useAlbedoMap: true cuando el JSON tiene campo 'albedo'") {
    const std::string name = uniqueName("uam_true");
    const std::string logical = writeMaterial(name, R"({
        "albedo": "textures/missing.png",
        "metallic": 0.0
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    // Aunque missing.png se resuelve a id 0, el JSON pidio explicitamente
    // textura -> el shader debe samplear (no caer al tint).
    CHECK(mat->useAlbedoMap == true);

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("useAlbedoMap: false cuando el JSON no tiene campo 'albedo' (tint puro)") {
    // Caso oro_pulido / plastico_azul: solo tint + multiplicadores escalares.
    const std::string name = uniqueName("uam_false");
    const std::string logical = writeMaterial(name, R"({
        "albedo_tint": [1.0, 0.84, 0.41],
        "metallic": 1.0,
        "roughness": 0.1
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    CHECK(mat->useAlbedoMap == false);
    CHECK(mat->albedo == 0);

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("default material: useAlbedoMap=true para mostrar missing como warning") {
    AssetManager am("assets", nullFactory());
    MaterialAsset* def = am.getMaterial(am.missingMaterialId());
    REQUIRE(def != nullptr);
    // Una entidad que cae al default sin material asignado debe ver el
    // patron magenta de missing.png (no blanco puro disfrazado de feature).
    CHECK(def->useAlbedoMap == true);
    CHECK(def->albedo == 0);
}

TEST_CASE("loadMaterialFromTexture: useAlbedoMap=true (auto-wrapper)") {
    AssetManager am("assets", nullFactory());
    const TextureAssetId tex = am.loadTexture("textures/missing.png");
    const MaterialAssetId mid = am.loadMaterialFromTexture(tex);
    MaterialAsset* mat = am.getMaterial(mid);
    REQUIRE(mat != nullptr);
    CHECK(mat->useAlbedoMap == true);
    CHECK(mat->albedo == tex);
}

TEST_CASE("createMaterialFromTexture: materialPathOf devuelve el sentinel '__runtime_tex#'") {
    // Garantia para el serializer: el path debe permitir reconocer el
    // wrapper y persistir la textura subyacente. Si fuera vacio, la
    // entidad cargada cae al default y se rompe la persistencia.
    AssetManager am("assets", nullFactory());
    const TextureAssetId tex = am.loadTexture("textures/missing.png");
    const MaterialAssetId mid = am.createMaterialFromTexture(tex);
    CHECK(am.materialPathOf(mid) == "__runtime_tex#" + std::to_string(tex));
}

TEST_CASE("createMaterialFromTexture: cada llamada con la misma textura devuelve id distinto") {
    // Diferencia clave con loadMaterialFromTexture (cacheado): asi cada
    // entidad spawneada/dropeada tiene su propio material editable sin
    // contagiar a otras que usan la misma textura.
    AssetManager am("assets", nullFactory());
    const TextureAssetId tex = am.loadTexture("textures/missing.png");

    const MaterialAssetId id1 = am.createMaterialFromTexture(tex);
    const MaterialAssetId id2 = am.createMaterialFromTexture(tex);
    CHECK(id1 != id2);

    // Editar uno NO debe mutar el otro.
    MaterialAsset* m1 = am.getMaterial(id1);
    MaterialAsset* m2 = am.getMaterial(id2);
    REQUIRE(m1 != nullptr);
    REQUIRE(m2 != nullptr);
    m1->albedoTint = glm::vec3(1.0f, 0.0f, 0.0f);
    CHECK(m2->albedoTint.x == doctest::Approx(1.0f));
    CHECK(m2->albedoTint.y == doctest::Approx(1.0f));
    CHECK(m2->albedoTint.z == doctest::Approx(1.0f));

    // Defaults igual que loadMaterialFromTexture (mate completo).
    CHECK(m1->useAlbedoMap == true);
    CHECK(m1->albedo == tex);
    CHECK(m1->roughnessMult == doctest::Approx(1.0f));
    CHECK(m1->metallicMult == doctest::Approx(0.0f));
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

// ============================================================
// F2H21: saveMaterial — persistencia del material editado
// ============================================================

TEST_CASE("saveMaterial: round-trip - load, modify, save, reload") {
    const std::string name = uniqueName("save_roundtrip");
    const std::string logical = writeMaterial(name, R"({
        "albedo_tint": [1.0, 1.0, 1.0],
        "metallic": 0.0,
        "roughness": 0.5,
        "ao": 1.0
    })");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial(logical);
    REQUIRE(id != am.missingMaterialId());

    // Modificar todos los campos.
    MaterialAsset* mat = am.getMaterial(id);
    REQUIRE(mat != nullptr);
    mat->albedoTint    = glm::vec3(0.42f, 0.69f, 0.13f);
    mat->metallicMult  = 0.88f;
    mat->roughnessMult = 0.17f;
    mat->aoMult        = 0.66f;

    REQUIRE(am.saveMaterial(id));

    // Releer en un AssetManager nuevo (forzar lectura de disco).
    AssetManager am2("assets", nullFactory());
    MaterialAsset* reloaded = am2.getMaterial(am2.loadMaterial(logical));
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded->albedoTint.x   == doctest::Approx(0.42f));
    CHECK(reloaded->albedoTint.y   == doctest::Approx(0.69f));
    CHECK(reloaded->albedoTint.z   == doctest::Approx(0.13f));
    CHECK(reloaded->metallicMult   == doctest::Approx(0.88f));
    CHECK(reloaded->roughnessMult  == doctest::Approx(0.17f));
    CHECK(reloaded->aoMult         == doctest::Approx(0.66f));

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("saveMaterial: id 0 (default) rechazado") {
    AssetManager am("assets", nullFactory());
    // Slot 0 = default material — sentinel "__default_material".
    CHECK_FALSE(am.saveMaterial(0));
}

TEST_CASE("saveMaterial: id fuera de rango rechazado") {
    AssetManager am("assets", nullFactory());
    CHECK_FALSE(am.saveMaterial(99999));
}

TEST_CASE("saveMaterial: createMaterial (path runtime sentinel) rechazado") {
    AssetManager am("assets", nullFactory());
    MaterialAsset proto{};
    const MaterialAssetId id = am.createMaterial(proto);
    // createMaterial usa key sintetica `__runtime#<id>` — no persistible.
    CHECK_FALSE(am.saveMaterial(id));
}

// ============================================================
// F2H63: transparencia (BlendMode + opacity + ior + refractionStrength)
// ============================================================

TEST_CASE("loadMaterial F2H63: defaults Opaque cuando JSON sin blend_mode") {
    const std::string name = uniqueName("blend_default");
    const std::string logical = writeMaterial(name, R"({
        "metallic": 0.5
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    CHECK(mat->blendMode          == BlendMode::Opaque);
    CHECK(mat->opacity            == doctest::Approx(1.0f));
    CHECK(mat->ior                == doctest::Approx(1.0f));
    CHECK(mat->refractionStrength == doctest::Approx(0.0f));

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("loadMaterial F2H63: blend_mode 'translucent' con opacity + ior + refraction_strength") {
    const std::string name = uniqueName("blend_translucent");
    const std::string logical = writeMaterial(name, R"({
        "blend_mode": "translucent",
        "opacity": 0.3,
        "ior": 1.5,
        "refraction_strength": 0.6
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    CHECK(mat->blendMode          == BlendMode::Translucent);
    CHECK(mat->opacity            == doctest::Approx(0.3f));
    CHECK(mat->ior                == doctest::Approx(1.5f));
    CHECK(mat->refractionStrength == doctest::Approx(0.6f));

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("loadMaterial F2H63: blend_mode 'additive' carga sin opacity (default 1.0)") {
    const std::string name = uniqueName("blend_additive");
    const std::string logical = writeMaterial(name, R"({
        "blend_mode": "additive",
        "albedo_tint": [1.0, 0.5, 0.0]
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    CHECK(mat->blendMode == BlendMode::Additive);
    CHECK(mat->opacity   == doctest::Approx(1.0f));

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("loadMaterial F2H63: blend_mode string desconocido cae a Opaque") {
    const std::string name = uniqueName("blend_unknown");
    const std::string logical = writeMaterial(name, R"({
        "blend_mode": "yolo_super_blend"
    })");

    AssetManager am("assets", nullFactory());
    MaterialAsset* mat = am.getMaterial(am.loadMaterial(logical));
    REQUIRE(mat != nullptr);
    CHECK(mat->blendMode == BlendMode::Opaque);

    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name);
}

TEST_CASE("saveMaterial F2H63: Opaque no escribe campos blend (back-compat)") {
    const std::string name = uniqueName("save_opaque");
    const std::string logical = writeMaterial(name, R"({
        "metallic": 0.5
    })");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial(logical);
    REQUIRE(am.saveMaterial(id));

    const auto fs = std::filesystem::path("assets") / "materials" / name;
    std::string content;
    {
        std::ifstream in(fs);
        content.assign((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
    }
    // Material Opaque (default) NO debe ensuciar el JSON con campos
    // de blending — mismo patron que `friction` (Hito 34 A).
    CHECK(content.find("\"blend_mode\"")          == std::string::npos);
    CHECK(content.find("\"opacity\"")             == std::string::npos);
    CHECK(content.find("\"ior\"")                 == std::string::npos);
    CHECK(content.find("\"refraction_strength\"") == std::string::npos);

    std::error_code ec;
    std::filesystem::remove(fs, ec);
}

TEST_CASE("saveMaterial F2H63: Translucent roundtrip con todos los campos") {
    const std::string name = uniqueName("save_translucent_roundtrip");
    const std::string logical = writeMaterial(name, R"({
        "metallic": 0.0,
        "roughness": 0.1
    })");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial(logical);
    MaterialAsset* mat = am.getMaterial(id);
    REQUIRE(mat != nullptr);
    mat->blendMode          = BlendMode::Translucent;
    mat->opacity            = 0.42f;
    mat->ior                = 1.33f;
    mat->refractionStrength = 0.75f;

    REQUIRE(am.saveMaterial(id));

    AssetManager am2("assets", nullFactory());
    MaterialAsset* reloaded = am2.getMaterial(am2.loadMaterial(logical));
    REQUIRE(reloaded != nullptr);
    CHECK(reloaded->blendMode          == BlendMode::Translucent);
    CHECK(reloaded->opacity            == doctest::Approx(0.42f));
    CHECK(reloaded->ior                == doctest::Approx(1.33f));
    CHECK(reloaded->refractionStrength == doctest::Approx(0.75f));

    std::error_code ec;
    std::filesystem::remove(std::filesystem::path("assets") / "materials" / name, ec);
}

TEST_CASE("saveMaterial F2H63: Additive con opacity!=1 persiste opacity") {
    const std::string name = uniqueName("save_additive");
    const std::string logical = writeMaterial(name, R"({})");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial(logical);
    MaterialAsset* mat = am.getMaterial(id);
    REQUIRE(mat != nullptr);
    mat->blendMode = BlendMode::Additive;
    mat->opacity   = 0.85f;
    // ior/refractionStrength quedan en defaults (Additive no usa refraccion).

    REQUIRE(am.saveMaterial(id));

    const auto fs = std::filesystem::path("assets") / "materials" / name;
    std::string content;
    {
        std::ifstream in(fs);
        content.assign((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
    }
    CHECK(content.find("\"blend_mode\"")          != std::string::npos);
    CHECK(content.find("\"additive\"")            != std::string::npos);
    CHECK(content.find("\"opacity\"")             != std::string::npos);
    // ior y refraction_strength en defaults -> NO persisten.
    CHECK(content.find("\"ior\"")                 == std::string::npos);
    CHECK(content.find("\"refraction_strength\"") == std::string::npos);

    std::error_code ec;
    std::filesystem::remove(fs, ec);
}

TEST_CASE("saveMaterial: campos de textura ausentes en JSON cuando slot=0") {
    const std::string name = uniqueName("save_no_textures");
    const std::string logical = writeMaterial(name, R"({
        "metallic": 0.5
    })");

    AssetManager am("assets", nullFactory());
    const MaterialAssetId id = am.loadMaterial(logical);
    REQUIRE(am.saveMaterial(id));

    // Leer el JSON resultante en raw — los campos albedo/normal/etc.
    // NO deben aparecer (slot = 0). Lectura en un scope cerrado para
    // que el ifstream se destruya antes del remove (Windows: el remove
    // falla con handle abierto).
    const auto fs = std::filesystem::path("assets") / "materials" / name;
    std::string content;
    {
        std::ifstream in(fs);
        content.assign((std::istreambuf_iterator<char>(in)),
                          std::istreambuf_iterator<char>());
    }
    CHECK(content.find("\"albedo\"") == std::string::npos);
    CHECK(content.find("\"normal\"") == std::string::npos);
    CHECK(content.find("\"metallic_roughness\"") == std::string::npos);
    // metallic / roughness / ao SI estan (escalares siempre).
    CHECK(content.find("\"metallic\"") != std::string::npos);

    std::error_code ec;
    std::filesystem::remove(fs, ec);
}
