// Tests del SetTileCommand (F2H16). Round-trip del cambio de
// tile en GridMap + sync callback.

#include <doctest/doctest.h>

#include "editor/commands/SetTileCommand.h"
#include "engine/world/grid/GridMap.h"

using namespace Mood;

TEST_CASE("SetTileCommand: execute setea newType + newTex") {
    GridMap map(4, 4, 1.0f);
    REQUIRE(map.tileAt(2, 2) == TileType::Empty);

    int syncCalls = 0;
    SetTileCommand cmd(&map,
        [&](u32, u32, TextureAssetId) { ++syncCalls; },
        2, 2,
        TileType::Empty, 0,
        TileType::SolidWall, 7,
        "Pintar tile (2,2)");

    cmd.execute();
    CHECK(map.tileAt(2, 2) == TileType::SolidWall);
    CHECK(map.tileTextureAt(2, 2) == 7);
    CHECK(syncCalls == 1);
}

TEST_CASE("SetTileCommand: undo restaura oldType + oldTex") {
    GridMap map(4, 4, 1.0f);
    map.setTile(1, 1, TileType::SolidWall, 3);

    SetTileCommand cmd(&map, [](u32, u32, TextureAssetId){},
        1, 1,
        TileType::SolidWall, 3,
        TileType::Empty, 0);

    cmd.execute();
    REQUIRE(map.tileAt(1, 1) == TileType::Empty);

    cmd.undo();
    CHECK(map.tileAt(1, 1) == TileType::SolidWall);
    CHECK(map.tileTextureAt(1, 1) == 3);
}

TEST_CASE("SetTileCommand: redo (execute tras undo)") {
    GridMap map(4, 4, 1.0f);
    SetTileCommand cmd(&map, [](u32, u32, TextureAssetId){},
        0, 0,
        TileType::Empty, 0,
        TileType::SolidWall, 5);

    cmd.execute();
    cmd.undo();
    cmd.execute();
    CHECK(map.tileAt(0, 0) == TileType::SolidWall);
    CHECK(map.tileTextureAt(0, 0) == 5);
}

TEST_CASE("SetTileCommand: name() devuelve label") {
    GridMap map(4, 4, 1.0f);
    SetTileCommand cmd(&map, {}, 0, 0,
        TileType::Empty, 0,
        TileType::SolidWall, 1,
        "Mi pintura");
    CHECK(cmd.name() == "Mi pintura");
}

TEST_CASE("SetTileCommand: sync callback nulo no crashea") {
    GridMap map(4, 4, 1.0f);
    SetTileCommand cmd(&map, {}, 0, 0,  // sync = empty fn
        TileType::Empty, 0,
        TileType::SolidWall, 1);
    cmd.execute();
    cmd.undo();
    CHECK(map.tileAt(0, 0) == TileType::Empty);
}
