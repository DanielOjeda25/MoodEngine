// Tests del helper isTileModified + parseTileCoords (red de seguridad
// del fix del bug de save de tiles modificados).

#include <doctest/doctest.h>

#include "engine/assets/manager/AssetManager.h"
#include "engine/render/rhi/ITexture.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/TilePersistence.h"
#include "engine/world/grid/GridMap.h"

#include <memory>

using namespace Mood;

namespace {

class StubTextureTP : public ITexture {
public:
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
};

AssetManager::TextureFactory stubFactoryTP() {
    return [](const std::string&) { return std::make_unique<StubTextureTP>(); };
}

// Helper: crea un Tile_X_Y entity en el estado "default" exacto del
// rebuildSceneFromMap (scale = tileSize, material auto-generado de la
// textura del grid en (x, y)).
Entity createDefaultTile(Scene& s, const GridMap& map, AssetManager& assets,
                          u32 x, u32 y) {
    const std::string tag = "Tile_" + std::to_string(x) + "_" + std::to_string(y);
    Entity e = s.createEntity(tag);
    auto& t = e.getComponent<TransformComponent>();
    t.scale = glm::vec3(map.tileSize());
    const MaterialAssetId mat =
        assets.createMaterialFromTexture(map.tileTextureAt(x, y));
    e.addComponent<MeshRendererComponent>(0u,
        std::vector<MaterialAssetId>{mat});
    return e;
}

} // namespace

TEST_CASE("parseTileCoords: tags validos parsean OK") {
    u32 x = 0, y = 0;
    CHECK(parseTileCoords("Tile_0_0", x, y));
    CHECK(x == 0u);
    CHECK(y == 0u);

    CHECK(parseTileCoords("Tile_8_8", x, y));
    CHECK(x == 8u);
    CHECK(y == 8u);

    CHECK(parseTileCoords("Tile_15_3", x, y));
    CHECK(x == 15u);
    CHECK(y == 3u);
}

TEST_CASE("parseTileCoords: tags invalidos fallan") {
    u32 x = 0, y = 0;
    CHECK_FALSE(parseTileCoords(nullptr, x, y));
    CHECK_FALSE(parseTileCoords("Tile", x, y));        // sin sufijo
    CHECK_FALSE(parseTileCoords("Tile_", x, y));       // separador suelto
    CHECK_FALSE(parseTileCoords("Tile_8", x, y));      // sin Y
    CHECK_FALSE(parseTileCoords("Tile_8_", x, y));     // Y vacio
    CHECK_FALSE(parseTileCoords("Tile_a_b", x, y));    // no-numerico
    CHECK_FALSE(parseTileCoords("Wall_8_8", x, y));    // prefijo distinto
    CHECK_FALSE(parseTileCoords("Tile_8_8_extra", x, y));  // sufijo extra
}

TEST_CASE("isTileModified: tile default NO esta modificado") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);
    map.setTile(8u, 8u, TileType::SolidWall, assets.loadTexture("textures/brick.png"));

    Scene s;
    Entity tile = createDefaultTile(s, map, assets, 8u, 8u);
    CHECK_FALSE(isTileModified(tile, map, assets));
}

TEST_CASE("isTileModified: scale alterado MARCA como modificado") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);
    map.setTile(8u, 8u, TileType::SolidWall, assets.loadTexture("textures/brick.png"));

    Scene s;
    Entity tile = createDefaultTile(s, map, assets, 8u, 8u);
    auto& t = tile.getComponent<TransformComponent>();
    t.scale.y = 6.0f;  // estirado en Y
    CHECK(isTileModified(tile, map, assets));
}

TEST_CASE("isTileModified: material distinto MARCA como modificado") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);
    const auto brickTex = assets.loadTexture("textures/brick.png");
    map.setTile(8u, 8u, TileType::SolidWall, brickTex);

    Scene s;
    Entity tile = createDefaultTile(s, map, assets, 8u, 8u);

    // Cambiar el material a uno con textura distinta (grid.png).
    const auto gridTex = assets.loadTexture("textures/grid.png");
    auto& mr = tile.getComponent<MeshRendererComponent>();
    mr.materials[0] = assets.createMaterialFromTexture(gridTex);

    CHECK(isTileModified(tile, map, assets));
}

TEST_CASE("isTileModified: tile sin MeshRenderer = modificado (caso raro)") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);
    map.setTile(8u, 8u, TileType::SolidWall, assets.loadTexture("textures/brick.png"));

    Scene s;
    Entity tile = s.createEntity("Tile_8_8");
    auto& t = tile.getComponent<TransformComponent>();
    t.scale = glm::vec3(map.tileSize());
    // SIN MeshRenderer.
    CHECK(isTileModified(tile, map, assets));
}

TEST_CASE("isTileModified: LightComponent extra MARCA como modificado") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);
    map.setTile(8u, 8u, TileType::SolidWall, assets.loadTexture("textures/brick.png"));

    Scene s;
    Entity tile = createDefaultTile(s, map, assets, 8u, 8u);
    LightComponent lc{};
    tile.addComponent<LightComponent>(lc);
    CHECK(isTileModified(tile, map, assets));
}

TEST_CASE("isTileModified: ScriptComponent con path no-vacio MARCA como modificado") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);
    map.setTile(8u, 8u, TileType::SolidWall, assets.loadTexture("textures/brick.png"));

    Scene s;
    Entity tile = createDefaultTile(s, map, assets, 8u, 8u);
    tile.addComponent<ScriptComponent>(std::string{"assets/scripts/foo.lua"});
    CHECK(isTileModified(tile, map, assets));
}

TEST_CASE("isTileModified: tag invalido = modificado (defensivo)") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);
    map.setTile(8u, 8u, TileType::SolidWall, assets.loadTexture("textures/brick.png"));

    Scene s;
    Entity weird = s.createEntity("NoEsTile");
    auto& t = weird.getComponent<TransformComponent>();
    t.scale = glm::vec3(3.0f);
    CHECK(isTileModified(weird, map, assets));
}

TEST_CASE("isTileModified: tile fuera del grid = modificado (defensivo)") {
    AssetManager assets("assets", stubFactoryTP());
    GridMap map(16u, 16u, 3.0f);

    Scene s;
    Entity outside = s.createEntity("Tile_99_99");
    auto& t = outside.getComponent<TransformComponent>();
    t.scale = glm::vec3(map.tileSize());
    CHECK(isTileModified(outside, map, assets));
}
