// Tests del FLUJO COMPLETO de save → load → applyEntitiesToScene.
//
// Los tests existentes en test_scene_serializer.cpp solo validan que el
// JSON se parsea correctamente. Estos tests validan algo mas profundo:
// que tras un round-trip completo, el SCENE LIVE (lo que ve el editor)
// queda equivalente al original.
//
// **Por que esto es necesario para un motor que crece**: cuando se agrega
// un componente nuevo, el dev tiene que:
//   1) Serializarlo en EntitySerializer.cpp (save).
//   2) Deserializarlo en EntitySerializer.cpp (parseEntity).
//   3) Aplicarlo al scene en SceneLoader.cpp (applyEntitiesToScene).
//   4) AGREGAR un test al template de abajo.
// Si se olvidan el paso 3 o 4, el bug es invisible — los tests viejos
// pasan pero el editor pierde data en el round-trip. Estos tests son la
// red de seguridad.
//
// Los tests cubren tres escenarios reales del editor:
// A) Entidad spawneada vía menú (no es tile) — debe round-trip exacto.
// B) Tile modificado: hipótesis del bug actual; verificamos qué se pierde.
// C) Entidad con materiales custom + Transform alterado.

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/resources/MaterialAsset.h"  // resolvedMat->albedo
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/grid/GridMap.h"

#include <filesystem>
#include <memory>
#include <string>

using namespace Mood;

namespace {

class StubTextureRT : public ITexture {
public:
    explicit StubTextureRT(std::string p) : m_path(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
private:
    std::string m_path;
};

AssetManager::TextureFactory stubFactoryRT() {
    return [](const std::string& p) { return std::make_unique<StubTextureRT>(p); };
}

std::filesystem::path tempPathRT(const char* suffix) {
    return std::filesystem::temp_directory_path() /
        (std::string("moodengine_full_rt_") + suffix);
}

// Helper para encontrar la primera entidad con un tag dado en el scene.
Entity findByTag(Scene& scene, const std::string& tag) {
    Entity result;
    scene.forEach<TagComponent>([&](Entity e, TagComponent& tc) {
        if (tc.name == tag && !static_cast<bool>(result)) {
            result = e;
        }
    });
    return result;
}

} // namespace

// ============================================================================
// Caso A: entidad spawneada NO-tile, Transform.scale + material modificados.
// Es el caso mas comun: el dev hace "Ayuda → Spawn caja fisica", la estira
// y le pone un material. Esperamos round-trip 100%.
// ============================================================================

TEST_CASE("Full round-trip: entidad NO-tile preserva Transform.scale + material") {
    AssetManager assets("assets", stubFactoryRT());
    const TextureAssetId brickTex = assets.loadTexture("textures/brick.png");
    const MaterialAssetId brickMat = assets.createMaterialFromTexture(brickTex);

    // 1) Construir scene origen con la entidad estirada + material asignado.
    Scene origScene;
    {
        Entity caja = origScene.createEntity("CajaFisica_01");
        auto& t = caja.getComponent<TransformComponent>();
        t.position = glm::vec3(1.0f, 2.0f, 3.0f);
        t.scale    = glm::vec3(2.5f, 0.5f, 1.5f);  // <-- estirada
        caja.addComponent<MeshRendererComponent>(
            assets.missingMeshId(),
            std::vector<MaterialAssetId>{brickMat});
    }

    // 2) Save al disco.
    const auto path = tempPathRT("entity_scale_material.moodmap");
    GridMap emptyMap(1u, 1u, 1.0f);
    SceneSerializer::save(emptyMap, "test", &origScene, assets, path);

    // 3) Load + applyEntitiesToScene a un scene NUEVO.
    const auto saved = SceneSerializer::load(path, assets);
    REQUIRE(saved.has_value());
    Scene reloadedScene;
    SceneLoader::applyEntitiesToScene(*saved, reloadedScene, assets);

    // 4) Verificar que la entidad en el scene reloaded tiene los valores
    //    originales (Transform.scale + material).
    Entity reloaded = findByTag(reloadedScene, "CajaFisica_01");
    REQUIRE(static_cast<bool>(reloaded));
    REQUIRE(reloaded.hasComponent<TransformComponent>());
    REQUIRE(reloaded.hasComponent<MeshRendererComponent>());

    const auto& t = reloaded.getComponent<TransformComponent>();
    CHECK(t.position.x == doctest::Approx(1.0f));
    CHECK(t.position.y == doctest::Approx(2.0f));
    CHECK(t.position.z == doctest::Approx(3.0f));
    CHECK(t.scale.x == doctest::Approx(2.5f));   // <-- estiramiento debe persistir
    CHECK(t.scale.y == doctest::Approx(0.5f));
    CHECK(t.scale.z == doctest::Approx(1.5f));

    const auto& mr = reloaded.getComponent<MeshRendererComponent>();
    REQUIRE(mr.materials.size() == 1u);
    // El material reloaded NO va a ser el mismo MaterialAssetId que el
    // original (cada load crea instances unicos via createMaterialFromTexture
    // segun el comentario en SceneLoader linea 49). Pero el material
    // resultante debe apuntar a la misma textura "brick.png".
    const MaterialAsset* resolvedMat = assets.getMaterial(mr.materials[0]);
    REQUIRE(resolvedMat != nullptr);
    const std::string albedoPath = assets.pathOf(resolvedMat->albedo);
    CHECK(albedoPath == "textures/brick.png");

    std::filesystem::remove(path);
}

// ============================================================================
// Caso B: tile del grid MODIFICADO. Tras el fix de isTileModified, los
// tiles modificados (scale != tileSize, material != tileTextureAt(x,y),
// componentes extras) AHORA se persisten al `.moodmap` como entidades.
// Tiles NO modificados se reconstruyen del grid al cargar (sin overhead
// en el JSON).
// ============================================================================

TEST_CASE("Full round-trip: tile MODIFICADO se persiste y se restaura via applyEntitiesToScene") {
    AssetManager assets("assets", stubFactoryRT());
    const TextureAssetId brickTex = assets.loadTexture("textures/brick.png");

    // 1) Construir scene origen con un Tile_5_5 MODIFICADO (estirado en Y).
    //    Simula lo que hace `rebuildSceneFromMap` para el tile original:
    //    scale = (tileSize, tileSize, tileSize), material = createMaterial
    //    de la textura del grid en (5,5). Despues lo "estiramos".
    Scene origScene;
    GridMap map(10u, 10u, 3.0f);  // tileSize = 3
    map.setTile(5u, 5u, TileType::SolidWall, brickTex);
    {
        Entity tile = origScene.createEntity("Tile_5_5");
        auto& t = tile.getComponent<TransformComponent>();
        t.position = glm::vec3(15.0f, 1.5f, 15.0f);
        // Default scale = tileSize (3). El user lo estira en Y a 6m.
        t.scale    = glm::vec3(3.0f, 6.0f, 3.0f);
        const MaterialAssetId mat =
            assets.createMaterialFromTexture(brickTex);
        tile.addComponent<MeshRendererComponent>(
            assets.missingMeshId(),
            std::vector<MaterialAssetId>{mat});
    }

    // 2) Save.
    const auto path = tempPathRT("tile_modified.moodmap");
    SceneSerializer::save(map, "tile_test", &origScene, assets, path);

    // 3) Load.
    const auto saved = SceneSerializer::load(path, assets);
    REQUIRE(saved.has_value());

    // 4) El Tile_5_5 modificado debe estar en `entities[]`.
    bool foundTile = false;
    glm::vec3 savedScale(0.0f);
    for (const auto& se : saved->entities) {
        if (se.tag == "Tile_5_5") {
            foundTile = true;
            savedScale = se.scale;
        }
    }
    CHECK(foundTile);
    CHECK(savedScale.y == doctest::Approx(6.0f));

    // 5) applyEntitiesToScene debe crear el tile en el scene live con
    //    los valores modificados. Si una entidad Tile_5_5 ya existe
    //    (caso real: rebuildSceneFromMap construyo un default antes),
    //    debe ser reemplazada.
    Scene reloadedScene;
    // Simulamos el flow del load: rebuildSceneFromMap crea un tile default.
    Entity defaultTile = reloadedScene.createEntity("Tile_5_5");
    {
        auto& t = defaultTile.getComponent<TransformComponent>();
        t.scale = glm::vec3(3.0f);  // default
    }
    SceneLoader::applyEntitiesToScene(*saved, reloadedScene, assets);

    // 6) Validar que el tile reaparece estirado y NO duplicado.
    int tileCount = 0;
    Entity reloadedTile;
    reloadedScene.forEach<TagComponent>([&](Entity e, TagComponent& tag) {
        if (tag.name == "Tile_5_5") {
            ++tileCount;
            reloadedTile = e;
        }
    });
    CHECK(tileCount == 1);  // SIN duplicacion
    REQUIRE(static_cast<bool>(reloadedTile));
    const auto& t = reloadedTile.getComponent<TransformComponent>();
    CHECK(t.scale.y == doctest::Approx(6.0f));  // estiramiento preservado

    std::filesystem::remove(path);
}

TEST_CASE("Full round-trip: tile NO modificado NO se persiste (no infla el JSON)") {
    AssetManager assets("assets", stubFactoryRT());
    const TextureAssetId brickTex = assets.loadTexture("textures/brick.png");

    // Tile en estado default exacto (scale = tileSize, material matchea
    // grid texture). El SceneSerializer NO debe persistirlo.
    Scene origScene;
    GridMap map(10u, 10u, 3.0f);
    map.setTile(5u, 5u, TileType::SolidWall, brickTex);
    {
        Entity tile = origScene.createEntity("Tile_5_5");
        auto& t = tile.getComponent<TransformComponent>();
        t.scale = glm::vec3(map.tileSize());  // default
        const MaterialAssetId mat =
            assets.createMaterialFromTexture(map.tileTextureAt(5u, 5u));
        tile.addComponent<MeshRendererComponent>(
            assets.missingMeshId(),
            std::vector<MaterialAssetId>{mat});
    }

    const auto path = tempPathRT("tile_default.moodmap");
    SceneSerializer::save(map, "tile_default", &origScene, assets, path);

    const auto saved = SceneSerializer::load(path, assets);
    REQUIRE(saved.has_value());
    CHECK(saved->entities.size() == 0u);  // tile default NO persiste

    std::filesystem::remove(path);
}

// ============================================================================
// Caso C: red de seguridad arquitectonica — round-trip COMPLETO de cada
// componente serializable. Si alguien agrega un componente nuevo y olvida
// el paso `applyEntitiesToScene`, este test (extendido) lo va a detectar.
//
// Cubre actualmente: Transform, MeshRenderer, Light, RigidBody. Cuando se
// agregue un componente nuevo a EntitySerializer.cpp, AGREGAR aqui un
// case correspondiente.
// ============================================================================

TEST_CASE("Full round-trip: red de seguridad — Transform + Light se aplican al scene") {
    AssetManager assets("assets", stubFactoryRT());

    Scene origScene;
    {
        // Light directional con valores no-default para validar que TODOS
        // los campos se preservan tras el round-trip.
        Entity sun = origScene.createEntity("Sol");
        auto& t = sun.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 10.0f, 0.0f);
        LightComponent lc{};
        lc.type        = LightComponent::Type::Directional;
        lc.color       = glm::vec3(1.0f, 0.5f, 0.2f);
        lc.intensity   = 2.5f;
        lc.direction   = glm::normalize(glm::vec3(-0.4f, -1.0f, -0.3f));
        lc.castShadows = true;
        sun.addComponent<LightComponent>(lc);
    }

    const auto path = tempPathRT("safety_net_light.moodmap");
    GridMap emptyMap(1u, 1u, 1.0f);
    SceneSerializer::save(emptyMap, "lights", &origScene, assets, path);

    const auto saved = SceneSerializer::load(path, assets);
    REQUIRE(saved.has_value());
    Scene reloadedScene;
    SceneLoader::applyEntitiesToScene(*saved, reloadedScene, assets);

    Entity sun = findByTag(reloadedScene, "Sol");
    REQUIRE(static_cast<bool>(sun));
    REQUIRE(sun.hasComponent<LightComponent>());
    const auto& lc = sun.getComponent<LightComponent>();
    CHECK(lc.type == LightComponent::Type::Directional);
    CHECK(lc.color.r == doctest::Approx(1.0f));
    CHECK(lc.color.g == doctest::Approx(0.5f));
    CHECK(lc.intensity == doctest::Approx(2.5f));
    CHECK(lc.castShadows);

    std::filesystem::remove(path);
}

TEST_CASE("Full round-trip: red de seguridad — RigidBody se aplica al scene") {
    AssetManager assets("assets", stubFactoryRT());

    Scene origScene;
    {
        Entity caja = origScene.createEntity("CajaDinamica");
        auto& t = caja.getComponent<TransformComponent>();
        t.position = glm::vec3(0.0f, 5.0f, 0.0f);
        t.scale    = glm::vec3(0.5f);  // half-size
        RigidBodyComponent rb{};
        rb.type        = RigidBodyComponent::Type::Dynamic;
        rb.shape       = RigidBodyComponent::Shape::Sphere;
        rb.halfExtents = glm::vec3(0.5f);
        rb.mass        = 7.5f;
        rb.friction    = 0.05f;  // hielo
        caja.addComponent<RigidBodyComponent>(rb);
    }

    const auto path = tempPathRT("safety_net_rigidbody.moodmap");
    GridMap emptyMap(1u, 1u, 1.0f);
    SceneSerializer::save(emptyMap, "physics", &origScene, assets, path);

    const auto saved = SceneSerializer::load(path, assets);
    REQUIRE(saved.has_value());
    Scene reloadedScene;
    SceneLoader::applyEntitiesToScene(*saved, reloadedScene, assets);

    Entity caja = findByTag(reloadedScene, "CajaDinamica");
    REQUIRE(static_cast<bool>(caja));
    REQUIRE(caja.hasComponent<RigidBodyComponent>());
    const auto& rb = caja.getComponent<RigidBodyComponent>();
    CHECK(rb.type == RigidBodyComponent::Type::Dynamic);
    CHECK(rb.shape == RigidBodyComponent::Shape::Sphere);
    CHECK(rb.mass == doctest::Approx(7.5f));
    CHECK(rb.friction == doctest::Approx(0.05f));

    const auto& t = caja.getComponent<TransformComponent>();
    CHECK(t.position.y == doctest::Approx(5.0f));
    CHECK(t.scale.x == doctest::Approx(0.5f));

    std::filesystem::remove(path);
}
