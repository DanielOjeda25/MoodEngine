// Tests de PrefabSerializer (Hito 14 Bloque 1).
// Round-trip: una entidad con Tag + Transform + MeshRenderer + Light se
// serializa a JSON, se relee y los valores quedan byte-a-byte. No requiere
// GL: el AssetManager se construye con factory mock y los meshes/texturas
// solo se referencian por path lógico.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"
#include "engine/scene/Components.h"
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/PrefabSerializer.h"

#include <nlohmann/json.hpp>

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

std::filesystem::path tempPath(const char* suffix) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        ("mood_prefab_test_" + std::to_string(stamp) + "_" + suffix);
}

} // namespace

TEST_CASE("PrefabSerializer: round-trip de entidad con MeshRenderer + Light") {
    AssetManager assets("assets", nullFactory());
    const TextureAssetId brick = assets.loadTexture("textures/brick.png");
    const MaterialAssetId brickMat = assets.loadMaterialFromTexture(brick);

    Scene scene;
    Entity e = scene.createEntity("Antorcha");
    auto& t = e.getComponent<TransformComponent>();
    t.position = glm::vec3(2.5f, 4.0f, -1.0f);
    t.rotationEuler = glm::vec3(0.0f, 90.0f, 0.0f);
    t.scale = glm::vec3(1.5f);
    e.addComponent<MeshRendererComponent>(MeshAssetId{0},
        std::vector<MaterialAssetId>{brickMat});
    LightComponent lc{};
    lc.type      = LightComponent::Type::Point;
    lc.color     = glm::vec3(1.0f, 0.4f, 0.1f);
    lc.intensity = 2.5f;
    lc.radius    = 6.0f;
    lc.enabled   = true;
    e.addComponent<LightComponent>(lc);

    const auto path = tempPath("torch.moodprefab");
    PrefabSerializer::save(e, "torch", assets, path);
    REQUIRE(std::filesystem::exists(path));

    const auto loaded = PrefabSerializer::load(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->name == "torch");
    CHECK(loaded->children.empty());

    const auto& sr = loaded->root;
    CHECK(sr.tag == "Antorcha");
    CHECK(sr.position.x == doctest::Approx(2.5f));
    CHECK(sr.position.y == doctest::Approx(4.0f));
    CHECK(sr.position.z == doctest::Approx(-1.0f));
    CHECK(sr.rotationEuler.y == doctest::Approx(90.0f));
    CHECK(sr.scale.x == doctest::Approx(1.5f));

    REQUIRE(sr.meshRenderer.has_value());
    CHECK(sr.meshRenderer->meshPath == "__missing_cube"); // mesh slot 0
    CHECK(sr.meshRenderer->materials.size() == 1);
    CHECK(sr.meshRenderer->materials[0] == "textures/brick.png");

    REQUIRE(sr.light.has_value());
    CHECK(sr.light->type == "point");
    CHECK(sr.light->intensity == doctest::Approx(2.5f));
    CHECK(sr.light->radius == doctest::Approx(6.0f));
    CHECK(sr.light->color.x == doctest::Approx(1.0f));
    CHECK(sr.light->color.y == doctest::Approx(0.4f));
    CHECK(sr.light->enabled);

    std::filesystem::remove(path);
}

TEST_CASE("PrefabSerializer: PrefabLink se persiste si la entidad lo tiene") {
    AssetManager assets("assets", nullFactory());
    Scene scene;
    Entity e = scene.createEntity("Linked");
    e.addComponent<PrefabLinkComponent>(std::string{"prefabs/torch.moodprefab"});

    const auto path = tempPath("linked.moodprefab");
    PrefabSerializer::save(e, "linked", assets, path);

    const auto loaded = PrefabSerializer::load(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->root.prefabPath == "prefabs/torch.moodprefab");

    std::filesystem::remove(path);
}

TEST_CASE("PrefabSerializer: load de archivo inexistente devuelve nullopt") {
    const auto missing = tempPath("no_existe.moodprefab");
    std::filesystem::remove(missing);
    CHECK_FALSE(PrefabSerializer::load(missing).has_value());
}

TEST_CASE("PrefabSerializer: version futura devuelve nullopt") {
    const auto path = tempPath("future.moodprefab");
    {
        nlohmann::json j;
        j["version"] = k_MoodprefabFormatVersion + 1;
        j["root"]    = nlohmann::json{{"tag", "x"}};
        std::ofstream out(path);
        out << j.dump();
    }
    CHECK_FALSE(PrefabSerializer::load(path).has_value());
    std::filesystem::remove(path);
}

TEST_CASE("PrefabSerializer: load JSON corrupto devuelve nullopt") {
    const auto path = tempPath("corrupto.moodprefab");
    {
        std::ofstream out(path);
        out << "{ esto no es json valido <<<";
    }
    CHECK_FALSE(PrefabSerializer::load(path).has_value());
    std::filesystem::remove(path);
}

// --- AssetManager::loadPrefab (Hito 14 Bloque 2) ---

TEST_CASE("AssetManager::loadPrefab cachea + cae a missing ante fallo") {
    AssetManager am("assets", nullFactory());

    // Slot 0 siempre existe y es el "prefab vacio".
    const auto* slot0 = am.getPrefab(am.missingPrefabId());
    REQUIRE(slot0 != nullptr);
    CHECK(slot0->name == "(empty)");

    // Path invalido (../) cae al missing via VFS.
    const auto bad = am.loadPrefab("../leak.moodprefab");
    CHECK(bad == am.missingPrefabId());

    // Path valido pero archivo inexistente: cae al missing y cachea para
    // no reintentar.
    const auto absent = am.loadPrefab("prefabs/no_existe.moodprefab");
    CHECK(absent == am.missingPrefabId());
    // segunda llamada usa el cache, mismo id.
    CHECK(am.loadPrefab("prefabs/no_existe.moodprefab") == am.missingPrefabId());

    // Id fuera de rango cae al missing.
    CHECK(am.getPrefab(9999) == slot0);
}

TEST_CASE("AssetManager::loadPrefab carga un prefab real escrito a disco") {
    // Escribimos el prefab a un path bajo `assets/prefabs/` (existe en el
    // repo) para que el VFS lo encuentre. Usamos un nombre temporal con
    // timestamp para no colisionar con assets reales.
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string fileName = "test_" + std::to_string(stamp) + ".moodprefab";
    const std::string logicalPath = "prefabs/" + fileName;
    const std::filesystem::path fsPath =
        std::filesystem::path("assets") / "prefabs" / fileName;
    std::filesystem::create_directories(fsPath.parent_path());

    {
        nlohmann::json j;
        j["version"] = k_MoodprefabFormatVersion;
        j["name"]    = "demo";
        j["root"]    = nlohmann::json{
            {"tag", "Hello"},
            {"transform", {
                {"position",      nlohmann::json::array({1.0, 2.0, 3.0})},
                {"rotationEuler", nlohmann::json::array({0.0, 0.0, 0.0})},
                {"scale",         nlohmann::json::array({1.0, 1.0, 1.0})}
            }}
        };
        j["children"] = nlohmann::json::array();
        std::ofstream out(fsPath);
        out << j.dump();
    }

    AssetManager am("assets", nullFactory());
    const auto id = am.loadPrefab(logicalPath);
    CHECK(id != am.missingPrefabId());

    const auto* sp = am.getPrefab(id);
    REQUIRE(sp != nullptr);
    CHECK(sp->name == "demo");
    CHECK(sp->root.tag == "Hello");
    CHECK(sp->root.position.x == doctest::Approx(1.0));

    // Cache: segunda load del mismo path = mismo id, no se duplica.
    CHECK(am.loadPrefab(logicalPath) == id);
    CHECK(am.prefabPathOf(id) == logicalPath);

    std::filesystem::remove(fsPath);
}
