// Tests de ProjectSerializer: round-trip de un Project con N mapas,
// manejo de archivos faltantes / corruptos / version futura, y la
// creacion de un proyecto nuevo con estructura de carpetas.

#include <doctest/doctest.h>

#include "engine/assets/AssetManager.h"
#include "engine/render/ITexture.h"
#include "engine/serialization/JsonHelpers.h"
#include "engine/serialization/ProjectSerializer.h"
#include "engine/serialization/SceneSerializer.h"
#include "engine/world/GridMap.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace fs = std::filesystem;
using namespace Mood;

namespace {

/// Carpeta temporal unica por test case. Se limpia al final de cada test.
fs::path tempDir(const char* suffix) {
    return fs::temp_directory_path() / (std::string("moodengine_proj_") + suffix);
}

/// Textura mock sin GL para poder crear AssetManagers en tests.
class NullTexture : public ITexture {
public:
    explicit NullTexture(std::string p) : m_p(std::move(p)) {}
    void bind(u32 = 0) const override {}
    void unbind() const override {}
    u32 width() const override { return 1; }
    u32 height() const override { return 1; }
    TextureHandle handle() const override { return nullptr; }
private:
    std::string m_p;
};

void nukeDir(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
}

} // namespace

TEST_CASE("ProjectSerializer: round-trip de un proyecto con 2 mapas") {
    const auto root = tempDir("roundtrip");
    nukeDir(root);
    fs::create_directories(root);

    Project original;
    original.root       = root;
    original.name       = "MiJuego";
    original.defaultMap = "maps/default.moodmap";
    original.maps       = {"maps/default.moodmap", "maps/level1.moodmap"};

    CHECK_NOTHROW(ProjectSerializer::save(original));

    const auto path = root / "MiJuego.moodproj";
    REQUIRE(fs::exists(path));

    const auto loaded = ProjectSerializer::load(path);
    REQUIRE(loaded.has_value());
    CHECK(loaded->name == original.name);
    CHECK(loaded->defaultMap == original.defaultMap);
    REQUIRE(loaded->maps.size() == original.maps.size());
    CHECK(loaded->maps[0] == original.maps[0]);
    CHECK(loaded->maps[1] == original.maps[1]);
    // root se infiere del path del archivo, no del JSON.
    CHECK(loaded->root == root);

    nukeDir(root);
}

TEST_CASE("ProjectSerializer: load archivo inexistente -> nullopt") {
    const auto path = tempDir("nope") / "no_existe.moodproj";
    CHECK_FALSE(ProjectSerializer::load(path).has_value());
}

TEST_CASE("ProjectSerializer: load JSON mal formado -> nullopt") {
    const auto root = tempDir("broken");
    nukeDir(root);
    fs::create_directories(root);
    const auto path = root / "broken.moodproj";
    {
        std::ofstream out(path);
        out << "{ no es json";
    }
    CHECK_FALSE(ProjectSerializer::load(path).has_value());
    nukeDir(root);
}

TEST_CASE("ProjectSerializer: load version futura -> nullopt") {
    const auto root = tempDir("future");
    nukeDir(root);
    fs::create_directories(root);
    const auto path = root / "future.moodproj";
    {
        nlohmann::json j;
        j["version"]    = k_MoodprojFormatVersion + 1;
        j["name"]       = "futuro";
        j["defaultMap"] = "maps/x.moodmap";
        j["maps"]       = nlohmann::json::array({"maps/x.moodmap"});
        std::ofstream out(path);
        out << j.dump();
    }
    CHECK_FALSE(ProjectSerializer::load(path).has_value());
    nukeDir(root);
}

TEST_CASE("ProjectSerializer: createNewProject crea carpetas y .moodproj") {
    const auto root = tempDir("new_project");
    nukeDir(root);

    const auto p = ProjectSerializer::createNewProject(root, "MiJuego");
    REQUIRE(p.has_value());
    CHECK(p->name == "MiJuego");
    CHECK(p->root == root);
    // Sin defaultMapName explicito, el mapa hereda el nombre del proyecto
    // (evita colisiones si dos proyectos comparten root).
    CHECK(p->defaultMap == fs::path("maps") / "MiJuego.moodmap");
    REQUIRE(p->maps.size() == 1);

    CHECK(fs::is_directory(root / "maps"));
    CHECK(fs::is_directory(root / "textures"));
    CHECK(fs::is_regular_file(root / "MiJuego.moodproj"));

    // Releer el proyecto desde disco y comparar.
    const auto reloaded = ProjectSerializer::load(root / "MiJuego.moodproj");
    REQUIRE(reloaded.has_value());
    CHECK(reloaded->name == "MiJuego");
    CHECK(reloaded->defaultMap == p->defaultMap);

    nukeDir(root);
}

TEST_CASE("ProjectSerializer: save con root vacio lanza") {
    Project p;
    p.name = "X";
    CHECK_THROWS_AS(ProjectSerializer::save(p), std::runtime_error);
}

TEST_CASE("ProjectSerializer: save con name vacio lanza") {
    Project p;
    p.root = tempDir("noname");
    nukeDir(p.root);
    fs::create_directories(p.root);
    CHECK_THROWS_AS(ProjectSerializer::save(p), std::runtime_error);
    nukeDir(p.root);
}

TEST_CASE("ProjectSerializer: dos proyectos en la misma carpeta no colisionan") {
    // Regresion del bug donde dos createNewProject(root, name) en la misma
    // `root` pisaban mutuamente el mapa default (ambos usaban
    // `maps/default.moodmap`). Ahora el mapa hereda el nombre del proyecto
    // salvo que se pase defaultMapName explicito.
    const auto root = tempDir("shared_root");
    nukeDir(root);
    fs::create_directories(root);

    const auto a = ProjectSerializer::createNewProject(root, "ProyA");
    const auto b = ProjectSerializer::createNewProject(root, "ProyB");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());

    // Paths de .moodproj distintos...
    CHECK(a->defaultMap != b->defaultMap);
    CHECK(a->defaultMap == fs::path("maps") / "ProyA.moodmap");
    CHECK(b->defaultMap == fs::path("maps") / "ProyB.moodmap");

    // ...y al releer cada .moodproj queda la referencia correcta.
    const auto loadedA = ProjectSerializer::load(root / "ProyA.moodproj");
    const auto loadedB = ProjectSerializer::load(root / "ProyB.moodproj");
    REQUIRE(loadedA.has_value());
    REQUIRE(loadedB.has_value());
    CHECK(loadedA->defaultMap == fs::path("maps") / "ProyA.moodmap");
    CHECK(loadedB->defaultMap == fs::path("maps") / "ProyB.moodmap");

    nukeDir(root);
}

TEST_CASE("ProjectSerializer + SceneSerializer: round-trip de dos proyectos en la misma carpeta") {
    // Extension del test anterior: si guardamos un mapa distinto en cada
    // proyecto y luego releemos los dos, cada uno debe ver su propio
    // contenido. Cubre el bug donde el mapa del segundo proyecto machacaba
    // el del primero.
    const auto root = tempDir("cross_contamination");
    nukeDir(root);
    fs::create_directories(root);

    AssetManager assets("assets",
        [](const std::string& p) -> std::unique_ptr<ITexture> {
            return std::make_unique<NullTexture>(p);
        });
    const TextureAssetId t1 = assets.loadTexture("textures/grid.png");
    const TextureAssetId t2 = assets.loadTexture("textures/brick.png");

    // Proyecto A con un tile en (0,0)
    const auto a = ProjectSerializer::createNewProject(root, "A");
    REQUIRE(a.has_value());
    GridMap mapA(4u, 4u, 1.0f);
    mapA.setTile(0, 0, TileType::SolidWall, t1);
    SceneSerializer::save(mapA, "A", assets, a->root / a->defaultMap);

    // Proyecto B con un tile en (3,3) -- distinto a A
    const auto b = ProjectSerializer::createNewProject(root, "B");
    REQUIRE(b.has_value());
    GridMap mapB(4u, 4u, 1.0f);
    mapB.setTile(3, 3, TileType::SolidWall, t2);
    SceneSerializer::save(mapB, "B", assets, b->root / b->defaultMap);

    // Releer A y verificar que su tile sigue en (0,0), sin el de B.
    const auto reA = SceneSerializer::load(a->root / a->defaultMap, assets);
    REQUIRE(reA.has_value());
    CHECK(reA->map.isSolid(0, 0));
    CHECK_FALSE(reA->map.isSolid(3, 3));

    // Releer B y verificar el opuesto.
    const auto reB = SceneSerializer::load(b->root / b->defaultMap, assets);
    REQUIRE(reB.has_value());
    CHECK_FALSE(reB->map.isSolid(0, 0));
    CHECK(reB->map.isSolid(3, 3));

    nukeDir(root);
}
