// Tests de SceneSerializer: round-trip completo de un mapa pequeno,
// manejo de archivos inexistentes / mal formados / version futura.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/JsonHelpers.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/grid/GridMap.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

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
    const std::string& path() const { return m_p; }
private:
    std::string m_p;
};

AssetManager::TextureFactory nullFactory() {
    return [](const std::string& p) { return std::make_unique<NullTexture>(p); };
}

/// Ruta temporal dentro del temp del sistema. Cada test usa un suffix
/// distinto y borra el archivo al terminar para no ensuciar entre
/// corridas.
std::filesystem::path tempPath(const char* suffix) {
    return std::filesystem::temp_directory_path() /
        (std::string("moodengine_test_") + suffix);
}

} // namespace

TEST_CASE("SceneSerializer: round-trip de un mapa 3x3 con texturas distintas") {
    AssetManager assets("assets", nullFactory());
    const TextureAssetId grid  = assets.loadTexture("textures/grid.png");
    const TextureAssetId brick = assets.loadTexture("textures/brick.png");

    GridMap original(3u, 3u, 2.5f);
    original.setTile(0, 0, TileType::SolidWall, grid);
    original.setTile(2, 0, TileType::SolidWall, brick);
    original.setTile(1, 1, TileType::SolidWall, grid);
    // resto queda Empty con texture = 0 (missing)

    const auto path = tempPath("map_roundtrip.moodmap");
    SceneSerializer::save(original, "prueba", nullptr, assets, path);
    REQUIRE(std::filesystem::exists(path));

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    CHECK(loaded->name == "prueba");
    CHECK(loaded->map.width() == original.width());
    CHECK(loaded->map.height() == original.height());
    CHECK(loaded->map.tileSize() == doctest::Approx(original.tileSize()));

    for (u32 y = 0; y < original.height(); ++y) {
        for (u32 x = 0; x < original.width(); ++x) {
            CAPTURE(x); CAPTURE(y);
            CHECK(loaded->map.tileAt(x, y) == original.tileAt(x, y));
            CHECK(loaded->map.tileTextureAt(x, y) == original.tileTextureAt(x, y));
        }
    }

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: load archivo inexistente devuelve nullopt") {
    AssetManager assets("assets", nullFactory());
    const auto missing = tempPath("no_existe.moodmap");
    std::filesystem::remove(missing); // por las dudas
    const auto r = SceneSerializer::load(missing, assets);
    CHECK_FALSE(r.has_value());
}

TEST_CASE("SceneSerializer: load JSON mal formado devuelve nullopt") {
    const auto path = tempPath("broken.moodmap");
    {
        std::ofstream out(path);
        out << "{ esto no es json valido";
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: load version futura devuelve nullopt") {
    const auto path = tempPath("future.moodmap");
    {
        nlohmann::json j;
        j["version"]  = k_MoodmapFormatVersion + 1;
        j["name"]     = "from_future";
        j["width"]    = 1;
        j["height"]   = 1;
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", "textures/missing.png"}}
        });
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: load tiles count mismatch devuelve nullopt") {
    const auto path = tempPath("mismatch.moodmap");
    {
        nlohmann::json j;
        j["version"]  = k_MoodmapFormatVersion;
        j["name"]     = "bad";
        j["width"]    = 2;
        j["height"]   = 2; // esperaria 4 tiles
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", ""}},
            {{"type", "empty"}, {"texture", ""}}
            // faltan 2 tiles
        });
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    CHECK_FALSE(r.has_value());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: save escribe un JSON parseable con los campos clave") {
    AssetManager assets("assets", nullFactory());
    GridMap m(2u, 1u, 3.0f);
    m.setTile(0, 0, TileType::SolidWall, assets.loadTexture("textures/grid.png"));

    const auto path = tempPath("parseable.moodmap");
    SceneSerializer::save(m, "inspeccion", nullptr, assets, path);

    nlohmann::json j;
    {
        std::ifstream in(path);
        in >> j;
    }
    CHECK(j.at("version") == k_MoodmapFormatVersion);
    CHECK(j.at("name") == "inspeccion");
    CHECK(j.at("width") == 2);
    CHECK(j.at("height") == 1);
    REQUIRE(j.at("tiles").size() == 2);
    CHECK(j.at("tiles")[0].at("type") == "solid_wall");
    CHECK(j.at("tiles")[0].at("texture") == "textures/grid.png");
    CHECK(j.at("tiles")[1].at("type") == "empty");

    std::filesystem::remove(path);
}

// --- Round-trip de entidades no-tile (Hito 10 Bloque 6) ---

TEST_CASE("SceneSerializer: round-trip entidades con MeshRenderer (Hito 10)") {
    AssetManager assets("assets", nullFactory());
    const TextureAssetId brick = assets.loadTexture("textures/brick.png");
    const MaterialAssetId brickMat = assets.loadMaterialFromTexture(brick);

    // Scene con 2 entidades no-tile + una tile-like (que NO deberia
    // serializarse por el filtro de prefijo "Tile_").
    Scene scene;
    {
        Entity a = scene.createEntity("Mesh_pyramid");
        auto& ta = a.getComponent<TransformComponent>();
        ta.position = glm::vec3(1.0f, 2.0f, 3.0f);
        ta.rotationEuler = glm::vec3(45.0f, 0.0f, 0.0f);
        ta.scale = glm::vec3(2.0f);
        a.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{brickMat, brickMat});

        // Una tile-like que debe ser FILTRADA (solo por prefijo del tag).
        Entity t = scene.createEntity("Tile_0_0");
        t.addComponent<MeshRendererComponent>(MeshAssetId{0}, MaterialAssetId{0});

        // Sin MeshRenderer: tambien filtrada (no tiene nada para serializar).
        Entity z = scene.createEntity("vacio");
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("entities_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1); // solo "Mesh_pyramid" entra

    const auto& se = loaded->entities[0];
    CHECK(se.tag == "Mesh_pyramid");
    CHECK(se.position.x == doctest::Approx(1.0f));
    CHECK(se.position.y == doctest::Approx(2.0f));
    CHECK(se.position.z == doctest::Approx(3.0f));
    CHECK(se.rotationEuler.x == doctest::Approx(45.0f));
    CHECK(se.scale.x == doctest::Approx(2.0f));
    REQUIRE(se.meshRenderer.has_value());
    // El mesh id 0 es el cubo fallback; path logico = "__missing_cube".
    CHECK(se.meshRenderer->meshPath == "__missing_cube");
    CHECK(se.meshRenderer->materials.size() == 2);
    // Hito 17: el material auto-generado a partir de una textura se
    // persiste como el path de la textura subyacente para back-compat
    // con el .moodmap v6 (los archivos viejos siguen leyendose como
    // wrappers en runtime; los nuevos no necesitan un .material explicito).
    CHECK(se.meshRenderer->materials[0] == "textures/brick.png");

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de EnvironmentComponent (Hito 15)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    Entity env = scene.createEntity("Environment");
    EnvironmentComponent env_data{};
    env_data.skyboxPath     = "skyboxes/sky_night";
    env_data.fogMode        = 2; // Exp
    env_data.fogColor       = glm::vec3(0.8f, 0.4f, 0.1f);
    env_data.fogDensity     = 0.05f;
    env_data.fogLinearStart = 7.5f;
    env_data.fogLinearEnd   = 42.0f;
    env_data.exposure       = 1.5f;
    env_data.tonemapMode    = 1; // Reinhard
    env.addComponent<EnvironmentComponent>(env_data);

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("env_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo_env", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);

    const auto& se = loaded->entities[0];
    CHECK(se.tag == "Environment");
    REQUIRE(se.environment.has_value());
    const auto& s = *se.environment;
    CHECK(s.skyboxPath     == "skyboxes/sky_night");
    CHECK(s.fogMode        == "exp");
    CHECK(s.fogColor.x     == doctest::Approx(0.8f));
    CHECK(s.fogColor.y     == doctest::Approx(0.4f));
    CHECK(s.fogColor.z     == doctest::Approx(0.1f));
    CHECK(s.fogDensity     == doctest::Approx(0.05f));
    CHECK(s.fogLinearStart == doctest::Approx(7.5f));
    CHECK(s.fogLinearEnd   == doctest::Approx(42.0f));
    CHECK(s.exposure       == doctest::Approx(1.5f));
    CHECK(s.tonemapMode    == "reinhard");

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: archivo v1 (sin 'entities') se carga con lista vacia") {
    // Simula un .moodmap viejo: sin campo `entities`. Debe seguir cargando.
    const auto path = tempPath("v1_legacy.moodmap");
    {
        nlohmann::json j;
        j["version"]  = 1; // version anterior
        j["name"]     = "legacy";
        j["width"]    = 1;
        j["height"]   = 1;
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", "textures/missing.png"}}
        });
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    REQUIRE(r.has_value());
    CHECK(r->entities.empty());
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: v6 con texture path en 'materials' carga con upgrader (Hito 17)") {
    // Simula un .moodmap v6 donde el campo `materials` del MeshRenderer
    // tenia paths de TEXTURA, no de material. El SavedMeshRenderer del
    // serializer es agnostico (es vector<string>); el upgrader que envuelve
    // texturas en materiales auto-generados vive en EditorProjectActions
    // (loadProject). Aca solo verificamos que el .moodmap v6 SE CARGA sin
    // explotar y que el path queda como string en el SavedEntity.
    const auto path = tempPath("v6_materials_as_textures.moodmap");
    {
        nlohmann::json j;
        j["version"]  = 6; // pre-Hito 17
        j["name"]     = "legacy_v6";
        j["width"]    = 1;
        j["height"]   = 1;
        j["tileSize"] = 1.0f;
        j["tiles"]    = nlohmann::json::array({
            {{"type", "empty"}, {"texture", "textures/missing.png"}}
        });
        nlohmann::json je;
        je["tag"] = "Mesh_v6";
        je["transform"] = {
            {"position", nlohmann::json::array({0.0f, 0.0f, 0.0f})},
            {"rotationEuler", nlohmann::json::array({0.0f, 0.0f, 0.0f})},
            {"scale", nlohmann::json::array({1.0f, 1.0f, 1.0f})}};
        // Aca esta la prueba: `materials` con un PATH DE TEXTURA (sin
        // `.material`). El upgrader del consumidor debe tratarlo como
        // textura y envolverlo en un material wrapper.
        je["mesh_renderer"] = {
            {"mesh_path", "__missing_cube"},
            {"materials", nlohmann::json::array({"textures/brick.png"})}};
        j["entities"] = nlohmann::json::array({je});
        std::ofstream out(path);
        out << j.dump();
    }
    AssetManager assets("assets", nullFactory());
    const auto r = SceneSerializer::load(path, assets);
    REQUIRE(r.has_value());
    REQUIRE(r->entities.size() == 1);
    REQUIRE(r->entities[0].meshRenderer.has_value());
    // El path se preserva tal cual en el SavedEntity (el upgrader vive
    // en el consumidor que monta los componentes en una Scene real).
    CHECK(r->entities[0].meshRenderer->materials.size() == 1);
    CHECK(r->entities[0].meshRenderer->materials[0] == "textures/brick.png");
    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de LightComponent (Hito 11)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    // Una entidad con Light Point + Transform; debe persistir todos los campos.
    Entity p = scene.createEntity("Luz_demo");
    {
        auto& t = p.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 4.0f, 0.0f);
        LightComponent lc{};
        lc.type      = LightComponent::Type::Point;
        lc.color     = glm::vec3(1.0f, 0.95f, 0.85f);
        lc.intensity = 1.5f;
        lc.radius    = 12.0f;
        lc.enabled   = true;
        p.addComponent<LightComponent>(lc);
    }
    // Una entidad Directional sin Transform-relevante (la dir va en el componente).
    Entity d = scene.createEntity("Sol");
    {
        LightComponent lc{};
        lc.type        = LightComponent::Type::Directional;
        lc.direction   = glm::vec3(0.0f, -1.0f, 0.3f);
        lc.color       = glm::vec3(1.0f, 0.8f, 0.6f);
        lc.intensity   = 0.5f;
        lc.enabled     = true;
        lc.castShadows = true; // Hito 16
        d.addComponent<LightComponent>(lc);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("light_roundtrip.moodmap");
    SceneSerializer::save(empty, "lit", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 2);

    // Buscar por tag (el orden de iteracion del registry no es estable).
    const SavedEntity* sePoint = nullptr;
    const SavedEntity* seDir   = nullptr;
    for (const auto& se : loaded->entities) {
        if (se.tag == "Luz_demo") sePoint = &se;
        if (se.tag == "Sol")      seDir   = &se;
    }
    REQUIRE(sePoint != nullptr);
    REQUIRE(seDir   != nullptr);
    REQUIRE(sePoint->light.has_value());
    REQUIRE(seDir->light.has_value());

    CHECK(sePoint->light->type == "point");
    CHECK(sePoint->light->intensity == doctest::Approx(1.5f));
    CHECK(sePoint->light->radius    == doctest::Approx(12.0f));
    CHECK(sePoint->light->color.r   == doctest::Approx(1.0f));
    CHECK(sePoint->light->color.g   == doctest::Approx(0.95f));
    CHECK(sePoint->light->enabled);
    // Point light no usa castShadows: el default false debe persistir.
    CHECK_FALSE(sePoint->light->castShadows);

    CHECK(seDir->light->type == "directional");
    CHECK(seDir->light->direction.y == doctest::Approx(-1.0f));
    CHECK(seDir->light->intensity   == doctest::Approx(0.5f));
    // Hito 16: castShadows=true debe persistir.
    CHECK(seDir->light->castShadows);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de RigidBodyComponent (Hito 12)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity box = scene.createEntity("CajaFisica");
    {
        auto& t = box.getComponent<TransformComponent>();
        t.position = glm::vec3(2.5f, 6.0f, -1.5f);
        t.scale = glm::vec3(1.0f);
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Dynamic;
        rb.shape = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(0.5f, 0.75f, 0.5f);
        rb.mass = 5.0f;
        box.addComponent<RigidBodyComponent>(rb);
    }

    Entity floor = scene.createEntity("Piso");
    {
        auto& t = floor.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f);
        RigidBodyComponent rb{};
        rb.type = RigidBodyComponent::Type::Static;
        rb.shape = RigidBodyComponent::Shape::Sphere;
        rb.halfExtents = glm::vec3(2.0f);
        floor.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("rigidbody_roundtrip.moodmap");
    SceneSerializer::save(empty, "physics", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 2);

    const SavedEntity* seBox = nullptr;
    const SavedEntity* seFloor = nullptr;
    for (const auto& se : loaded->entities) {
        if (se.tag == "CajaFisica") seBox = &se;
        if (se.tag == "Piso")       seFloor = &se;
    }
    REQUIRE(seBox   != nullptr);
    REQUIRE(seFloor != nullptr);
    REQUIRE(seBox->rigidBody.has_value());
    REQUIRE(seFloor->rigidBody.has_value());

    CHECK(seBox->rigidBody->type  == "dynamic");
    CHECK(seBox->rigidBody->shape == "box");
    CHECK(seBox->rigidBody->mass  == doctest::Approx(5.0f));
    CHECK(seBox->rigidBody->halfExtents.x == doctest::Approx(0.5f));
    CHECK(seBox->rigidBody->halfExtents.y == doctest::Approx(0.75f));

    CHECK(seFloor->rigidBody->type  == "static");
    CHECK(seFloor->rigidBody->shape == "sphere");
    CHECK(seFloor->rigidBody->halfExtents.x == doctest::Approx(2.0f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: friction custom value round-trip (Hito 34 A)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity ice = scene.createEntity("Hielo");
    {
        RigidBodyComponent rb{};
        rb.type        = RigidBodyComponent::Type::Static;
        rb.shape       = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(2.0f, 0.1f, 2.0f);
        rb.friction    = 0.05f;  // hielo
        ice.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("friction_custom.moodmap");
    SceneSerializer::save(empty, "ice", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1);
    REQUIRE(loaded->entities[0].rigidBody.has_value());
    CHECK(loaded->entities[0].rigidBody->friction == doctest::Approx(0.05f));

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: friction default (0.5) NO se persiste en JSON (Hito 34 A)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;

    Entity box = scene.createEntity("Caja");
    {
        RigidBodyComponent rb{};
        rb.type        = RigidBodyComponent::Type::Dynamic;
        rb.shape       = RigidBodyComponent::Shape::Box;
        rb.halfExtents = glm::vec3(0.5f);
        rb.mass        = 1.0f;
        // rb.friction queda en su default 0.5f.
        box.addComponent<RigidBodyComponent>(rb);
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("friction_default.moodmap");
    SceneSerializer::save(empty, "default", &scene, assets, path);

    // Leer el archivo raw y verificar que el bloque rigid_body NO contiene
    // un campo "friction" — back-compat con archivos pre-Hito 34 A.
    std::ifstream f(path);
    REQUIRE(f.is_open());
    nlohmann::json j = nlohmann::json::parse(f);
    f.close();
    REQUIRE(j.contains("entities"));
    REQUIRE(j["entities"].size() >= 1u);
    const auto& jent = j["entities"][0];
    REQUIRE(jent.contains("rigid_body"));
    CHECK_FALSE(jent["rigid_body"].contains("friction"));

    // Y el load igual recupera el default 0.5 (back-compat con archivos
    // pre-Hito 34 A que tampoco tenian el campo).
    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() >= 1u);
    REQUIRE(loaded->entities[0].rigidBody.has_value());
    CHECK(loaded->entities[0].rigidBody->friction == doctest::Approx(0.5f));

    std::filesystem::remove(path);
}

// ============================================================
// F2H48.1: DialogComponent round-trip
// ============================================================

TEST_CASE("SceneSerializer: round-trip de DialogComponent (F2H48.1)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_guard");
        DialogComponent dc{};
        dc.dialogPath           = "dialogs/intro.mooddialog";
        dc.autoStartOnInteract  = false;  // override del default
        dc.cachedDialogId       = 99;     // runtime; NO debe persistir
        npc.addComponent<DialogComponent>(dc);
        // MeshRenderer para que la entidad no sea filtrada por
        // tag-prefix (no es "Tile_").
        npc.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("dialog_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    CHECK(se.tag == "NPC_guard");
    REQUIRE(se.dialog.has_value());
    CHECK(se.dialog->dialogPath == "dialogs/intro.mooddialog");
    CHECK_FALSE(se.dialog->autoStartOnInteract);  // override preservado

    std::filesystem::remove(path);
}

// ============================================================
// F2H52: ItemPickupComponent round-trip — itemPath + quantity +
// destroyOnPickup sobreviven; cachedItemId runtime no se persiste.
// ============================================================

TEST_CASE("SceneSerializer: round-trip de ItemPickupComponent (F2H52)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity pickup = scene.createEntity("HealthPotion_drop");
        ItemPickupComponent ip{};
        ip.itemPath        = "items/health_potion.mooditem";
        ip.quantity        = 3;
        ip.destroyOnPickup = false;  // override del default (=true)
        ip.cachedItemId    = 42;     // runtime; NO debe persistir
        pickup.addComponent<ItemPickupComponent>(ip);
        // MeshRenderer para que la entity no caiga al filtrado tag-prefix.
        pickup.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("item_pickup_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    CHECK(se.tag == "HealthPotion_drop");
    REQUIRE(se.itemPickup.has_value());
    CHECK(se.itemPickup->itemPath == "items/health_potion.mooditem");
    CHECK(se.itemPickup->quantity == 3);
    CHECK_FALSE(se.itemPickup->destroyOnPickup);  // override preservado

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: ItemPickupComponent vacio NO se serializa (F2H52)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity pickup = scene.createEntity("EmptyPickup");
        ItemPickupComponent ip{};  // itemPath vacio
        pickup.addComponent<ItemPickupComponent>(ip);
        pickup.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("item_pickup_empty.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    // Convencion: pickup sin itemPath no entra al .moodmap (mismo patron
    // que DialogComponent vacio). Reload deja la entity sin el component.
    CHECK_FALSE(loaded->entities[0].itemPickup.has_value());

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: ItemPickupComponent defaults sensatos al reload (F2H52)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity pickup = scene.createEntity("MinimalPickup");
        ItemPickupComponent ip{};
        ip.itemPath = "items/key.mooditem";
        // quantity, destroyOnPickup quedan en default → no override en JSON.
        pickup.addComponent<ItemPickupComponent>(ip);
        pickup.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("item_pickup_defaults.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities[0].itemPickup.has_value());
    CHECK(loaded->entities[0].itemPickup->itemPath == "items/key.mooditem");
    CHECK(loaded->entities[0].itemPickup->quantity == 1);             // default
    CHECK(loaded->entities[0].itemPickup->destroyOnPickup == true);   // default

    std::filesystem::remove(path);
}

// ============================================================
// F2H50 Bloque D: AnimatorComponent persistence (clipName, speed, playing,
// loop, externalClips) sobrevive al roundtrip save+load.
// ============================================================

TEST_CASE("SceneSerializer: round-trip de AnimatorComponent (F2H50)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_anim");
        npc.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
        AnimatorComponent anim{};
        anim.clipName = "walk";
        anim.speed    = 1.5f;
        anim.playing  = false;
        anim.loop     = false;
        // 3 external clips usando los rigs demo Mixamo X Bot que vienen
        // commiteados en el repo (assets/characters/player/anim_*.fbx).
        // Necesitamos archivos reales porque el serializer usa
        // `animationClipPathOf(id)` que devuelve "__empty_clip" para ids
        // que cayeron al slot 0 (missing). Persistir paths reales valida
        // el roundtrip correctamente.
        anim.externalClips.emplace_back(
            "idle", assets.loadAnimationClip("characters/player/anim_idle.fbx"));
        anim.externalClips.emplace_back(
            "walk", assets.loadAnimationClip("characters/player/anim_walk.fbx"));
        anim.externalClips.emplace_back(
            "wave", assets.loadAnimationClip("characters/player/anim_wave.fbx"));
        npc.addComponent<AnimatorComponent>(std::move(anim));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("animator_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    REQUIRE(se.animator.has_value());
    CHECK(se.animator->clipName == "walk");
    CHECK(se.animator->speed == doctest::Approx(1.5f));
    CHECK_FALSE(se.animator->playing);
    CHECK_FALSE(se.animator->loop);
    REQUIRE(se.animator->externalClips.size() == 3u);
    CHECK(se.animator->externalClips[0].alias == "idle");
    CHECK(se.animator->externalClips[0].path == "characters/player/anim_idle.fbx");
    CHECK(se.animator->externalClips[1].alias == "walk");
    CHECK(se.animator->externalClips[1].path == "characters/player/anim_walk.fbx");
    CHECK(se.animator->externalClips[2].alias == "wave");
    CHECK(se.animator->externalClips[2].path == "characters/player/anim_wave.fbx");

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: AnimatorComponent sin externalClips se persiste igual (F2H50)") {
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity e = scene.createEntity("Anim_no_clips");
        e.addComponent<MeshRendererComponent>(
            MeshAssetId{0}, std::vector<MaterialAssetId>{0});
        AnimatorComponent anim{};
        anim.clipName = "embedded_clip";
        anim.playing  = true;
        anim.loop     = true;
        // externalClips vacio — caso comun de meshes con embedded animations
        // (Fox.glb, etc.). El roundtrip preserva los flags pero la lista
        // sigue vacia.
        e.addComponent<AnimatorComponent>(std::move(anim));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("animator_no_external_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    REQUIRE(se.animator.has_value());
    CHECK(se.animator->clipName == "embedded_clip");
    CHECK(se.animator->externalClips.empty());

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: DialogComponent con path vacio NO se persiste (F2H48.1)") {
    // Mismo patron que ScriptComponent: si path vacio, el componente
    // no se serializa (no aporta nada al round-trip).
    AssetManager assets("assets", nullFactory());

    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_sin_path");
        npc.addComponent<DialogComponent>();  // dialogPath = ""
        npc.addComponent<MeshRendererComponent>(MeshAssetId{0},
            std::vector<MaterialAssetId>{0});
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("dialog_empty_path.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    // El dialog NO debe estar en el JSON ni en el load.
    CHECK_FALSE(loaded->entities[0].dialog.has_value());

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: autoStartOnInteract default=true se persiste igual (F2H48.1)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;
    {
        Entity npc = scene.createEntity("NPC_default");
        DialogComponent dc{};
        dc.dialogPath = "dialogs/x.mooddialog";
        // autoStartOnInteract queda en true (default)
        npc.addComponent<DialogComponent>(dc);
        npc.addComponent<MeshRendererComponent>(MeshAssetId{0},
            std::vector<MaterialAssetId>{0});
    }
    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("dialog_default_flag.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);
    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities[0].dialog.has_value());
    CHECK(loaded->entities[0].dialog->autoStartOnInteract);
    std::filesystem::remove(path);
}

// ============================================================
// F2H51 Bloque I: InventoryComponent persistence — 3 layout modes
// ============================================================

TEST_CASE("SceneSerializer: round-trip de InventoryComponent FlatList (F2H51)") {
    AssetManager assets("assets", nullFactory());
    const ItemAssetId sword  = assets.loadItem("items/iron_sword.mooditem");
    const ItemAssetId potion = assets.loadItem("items/health_potion.mooditem");
    REQUIRE(sword != assets.missingItemId());
    REQUIRE(potion != assets.missingItemId());

    Scene scene;
    {
        Entity player = scene.createEntity("Player_inv");
        InventoryComponent inv{};
        inv.state.mode = Inventory::LayoutMode::FlatList;
        inv.state.config.max_items = 24;
        REQUIRE(inv.state.add(sword, 1, assets));
        REQUIRE(inv.state.add(potion, 5, assets));
        player.addComponent<InventoryComponent>(std::move(inv));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_flatlist_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    REQUIRE(loaded->entities.size() == 1u);
    const auto& se = loaded->entities[0];
    REQUIRE(se.inventory.has_value());
    CHECK(se.inventory->layoutMode == "flat_list");
    CHECK(se.inventory->maxItems == 24);
    REQUIRE(se.inventory->entries.size() == 2u);
    CHECK(se.inventory->entries[0].itemPath == "items/iron_sword.mooditem");
    CHECK(se.inventory->entries[0].quantity == 1);
    CHECK(se.inventory->entries[1].itemPath == "items/health_potion.mooditem");
    CHECK(se.inventory->entries[1].quantity == 5);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de InventoryComponent Grid2D (F2H51)") {
    AssetManager assets("assets", nullFactory());
    const ItemAssetId sword = assets.loadItem("items/iron_sword.mooditem");

    Scene scene;
    {
        Entity chest = scene.createEntity("Chest_inv");
        InventoryComponent inv{};
        inv.state.mode = Inventory::LayoutMode::Grid2D;
        inv.state.config.grid_width  = 3;
        inv.state.config.grid_height = 5;
        REQUIRE(inv.state.placeAt(sword, 1, 7, assets));
        chest.addComponent<InventoryComponent>(std::move(inv));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_grid_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const auto& se = loaded->entities[0];
    REQUIRE(se.inventory.has_value());
    CHECK(se.inventory->layoutMode == "grid_2d");
    CHECK(se.inventory->gridWidth == 3);
    CHECK(se.inventory->gridHeight == 5);
    REQUIRE(se.inventory->entries.size() == 1u);
    CHECK(se.inventory->entries[0].itemPath == "items/iron_sword.mooditem");
    CHECK(se.inventory->entries[0].slotIndex == 7);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: round-trip de InventoryComponent EquipmentSlots (F2H51)") {
    AssetManager assets("assets", nullFactory());
    const ItemAssetId sword = assets.loadItem("items/iron_sword.mooditem");

    Scene scene;
    {
        Entity hero = scene.createEntity("Hero_equip");
        InventoryComponent inv{};
        inv.state.mode = Inventory::LayoutMode::EquipmentSlots;
        inv.state.config.equipment_slots = {
            {"weapon_main", "weapon"},
            {"head",        "armor"},
            {"any",         ""},
        };
        REQUIRE(inv.state.placeAt(sword, 1, 0, assets));
        hero.addComponent<InventoryComponent>(std::move(inv));
    }

    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_equip_roundtrip.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);

    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    const auto& se = loaded->entities[0];
    REQUIRE(se.inventory.has_value());
    CHECK(se.inventory->layoutMode == "equipment_slots");
    REQUIRE(se.inventory->equipmentSlots.size() == 3u);
    CHECK(se.inventory->equipmentSlots[0].name == "weapon_main");
    CHECK(se.inventory->equipmentSlots[0].tagFilter == "weapon");
    CHECK(se.inventory->equipmentSlots[2].tagFilter.empty());
    REQUIRE(se.inventory->entries.size() == 1u);
    CHECK(se.inventory->entries[0].slotIndex == 0);

    std::filesystem::remove(path);
}

TEST_CASE("SceneSerializer: entidad sin InventoryComponent no persiste el campo (F2H51)") {
    AssetManager assets("assets", nullFactory());
    Scene scene;
    {
        Entity e = scene.createEntity("plain");
        e.addComponent<MeshRendererComponent>(MeshAssetId{0},
            std::vector<MaterialAssetId>{0});
        // sin InventoryComponent
    }
    GridMap empty(1u, 1u, 1.0f);
    const auto path = tempPath("inventory_absent.moodmap");
    SceneSerializer::save(empty, "demo", &scene, assets, path);
    const auto loaded = SceneSerializer::load(path, assets);
    REQUIRE(loaded.has_value());
    CHECK_FALSE(loaded->entities[0].inventory.has_value());
    std::filesystem::remove(path);
}
