// Tests de ProjectSerializer: round-trip de un Project con N mapas,
// manejo de archivos faltantes / corruptos / version futura, y la
// creacion de un proyecto nuevo con estructura de carpetas.

#include <doctest/doctest.h>

#include "engine/serialization/JsonHelpers.h"
#include "engine/serialization/ProjectSerializer.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace Mood;

namespace {

/// Carpeta temporal unica por test case. Se limpia al final de cada test.
fs::path tempDir(const char* suffix) {
    return fs::temp_directory_path() / (std::string("moodengine_proj_") + suffix);
}

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
    CHECK(p->defaultMap == fs::path("maps") / "default.moodmap");
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
