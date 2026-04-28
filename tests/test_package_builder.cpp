// Tests del PackageBuilder (Hito 21 Bloque 6).
//
// Headless: no requiere GL ni scene en memoria — solo I/O + JSON. El
// fixture arma un `engineExeDir` mock con archivos dummy que simulan
// MoodPlayer.exe + SDL2d.dll + assets/ + shaders/, y un `projectRoot`
// con un `.moodproj` + maps/. Despues invoca `PackageBuilder::build`
// y verifica que el output sea el esperado.

#include <doctest/doctest.h>

#include "engine/packaging/PackageBuilder.h"
#include "engine/serialization/ProjectSerializer.h"

#include <nlohmann/json.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace Mood;

namespace {

// Crea un dir temporal unico para cada test (evita colisiones si el
// runner paraleliza). Retorna el path; el destructor del scope dejaria
// el dir, asi que cada test que lo cree debe cleanup explicito al
// final si quiere ser cortes.
std::filesystem::path makeTempDir(const std::string& tag) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    auto p = std::filesystem::temp_directory_path() /
             ("mood_pkg_" + tag + "_" + std::to_string(stamp));
    std::filesystem::create_directories(p);
    return p;
}

// Escribe un archivo con `content` en `path`, creando los dirs padre.
void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path);
    f << content;
}

// Setea un engineExeDir mock con los archivos minimos que el packager
// busca: MoodPlayer.exe, SDL2d.dll, assets/foo.png, shaders/foo.vert.
std::filesystem::path makeMockEngineExeDir() {
    auto dir = makeTempDir("engine_exe");
    writeFile(dir / "MoodPlayer.exe",  "fake exe bytes");
    writeFile(dir / "SDL2d.dll",       "fake dll bytes");
    writeFile(dir / "assets" / "textures" / "missing.png", "png");
    writeFile(dir / "assets" / "audio"    / "missing.wav", "wav");
    writeFile(dir / "shaders" / "default.vert", "glsl");
    writeFile(dir / "shaders" / "default.frag", "glsl");
    return dir;
}

// Setea un Project mock con `<root>/<name>.moodproj` + `<root>/maps/<name>.moodmap`.
struct MockProject {
    Project project;
    std::filesystem::path moodprojPath;
    std::filesystem::path root;
};
MockProject makeMockProject(const std::string& name = "test_pkg") {
    MockProject m;
    m.root = makeTempDir("proj_" + name);
    m.moodprojPath = m.root / (name + ".moodproj");
    writeFile(m.moodprojPath, "{}"); // contenido no importa
    writeFile(m.root / "maps" / (name + ".moodmap"), "{}");
    m.project.name       = name;
    m.project.root       = m.root;
    m.project.defaultMap = std::filesystem::path("maps") / (name + ".moodmap");
    m.project.maps.push_back(m.project.defaultMap);
    return m;
}

void cleanupTree(const std::filesystem::path& p) {
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
}

} // namespace

TEST_CASE("PackageBuilder: build feliz produce el layout esperado") {
    auto engineExe = makeMockEngineExeDir();
    auto mock      = makeMockProject("game_ok");
    auto destDir   = makeTempDir("dest_ok");

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, destDir, engineExe, /*isDebug=*/true
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK(r.ok);
    CHECK(r.error.empty());
    CHECK(r.filesCopied > 0);
    CHECK(std::filesystem::exists(r.outputDir));

    // Layout esperado:
    CHECK(std::filesystem::exists(r.outputDir / "MoodPlayer.exe"));
    CHECK(std::filesystem::exists(r.outputDir / "SDL2d.dll"));
    CHECK(std::filesystem::exists(r.outputDir / "game.json"));
    CHECK(std::filesystem::exists(r.outputDir / "assets" / "textures" / "missing.png"));
    CHECK(std::filesystem::exists(r.outputDir / "shaders" / "default.vert"));
    CHECK(std::filesystem::exists(r.outputDir / "project" / "game_ok.moodproj"));
    CHECK(std::filesystem::exists(r.outputDir / "project" / "maps" / "game_ok.moodmap"));

    // game.json valido + apunta al .moodproj copiado.
    std::ifstream f(r.outputDir / "game.json");
    nlohmann::json j;
    f >> j;
    CHECK(j.value("version", 0) == 1);
    CHECK(j.value("name", std::string{}) == "game_ok");
    CHECK(j.value("project", std::string{}) == "project/game_ok.moodproj");
    CHECK(j.value("default_map", std::string{}) == "maps/game_ok.moodmap");

    cleanupTree(engineExe);
    cleanupTree(mock.root);
    cleanupTree(destDir);
}

TEST_CASE("PackageBuilder: rechaza dest dentro de project root") {
    auto engineExe = makeMockEngineExeDir();
    auto mock      = makeMockProject("recursive");
    // destDir = subdir del proyecto -> outDir cae adentro del project
    // root -> recursion infinita si no se valida.
    const auto destDir = mock.root / "subdir";

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, destDir, engineExe, true
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());
    // El error debe mencionar el problema de destino dentro del proyecto.
    CHECK(r.error.find("dentro de la carpeta del proyecto") != std::string::npos);

    cleanupTree(engineExe);
    cleanupTree(mock.root);
}

TEST_CASE("PackageBuilder: rechaza dest == project root") {
    auto engineExe = makeMockEngineExeDir();
    auto mock      = makeMockProject("self");

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, mock.root, engineExe, true
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK_FALSE(r.ok);
    CHECK_FALSE(r.error.empty());

    cleanupTree(engineExe);
    cleanupTree(mock.root);
}

TEST_CASE("PackageBuilder: falla si engineExeDir no existe") {
    auto mock    = makeMockProject("no_engine");
    auto destDir = makeTempDir("dest_no_engine");

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, destDir,
        std::filesystem::path("/__no_existe_345987__"), true
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK_FALSE(r.ok);
    CHECK(r.error.find("engineExeDir") != std::string::npos);

    cleanupTree(mock.root);
    cleanupTree(destDir);
}

TEST_CASE("PackageBuilder: falla si shaders/ no existe en engineExeDir") {
    auto engineExe = makeMockEngineExeDir();
    // Borrar shaders/ a proposito.
    std::filesystem::remove_all(engineExe / "shaders");

    auto mock    = makeMockProject("no_shaders");
    auto destDir = makeTempDir("dest_no_shaders");

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, destDir, engineExe, true
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK_FALSE(r.ok);
    CHECK(r.error.find("shaders") != std::string::npos);

    cleanupTree(engineExe);
    cleanupTree(mock.root);
    cleanupTree(destDir);
}

TEST_CASE("PackageBuilder: assets/ faltante es warning, no error") {
    auto engineExe = makeMockEngineExeDir();
    std::filesystem::remove_all(engineExe / "assets");

    auto mock    = makeMockProject("no_assets");
    auto destDir = makeTempDir("dest_no_assets");

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, destDir, engineExe, true
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK(r.ok); // No es error fatal: el paquete saldra sin engine assets.
    CHECK(std::filesystem::exists(r.outputDir / "MoodPlayer.exe"));
    CHECK(std::filesystem::exists(r.outputDir / "shaders" / "default.vert"));
    CHECK_FALSE(std::filesystem::exists(r.outputDir / "assets"));

    cleanupTree(engineExe);
    cleanupTree(mock.root);
    cleanupTree(destDir);
}

TEST_CASE("PackageBuilder: rechaza project name vacio") {
    auto engineExe = makeMockEngineExeDir();
    auto mock      = makeMockProject("dummy");
    mock.project.name.clear(); // forzar invalido

    auto destDir = makeTempDir("dest_no_name");

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, destDir, engineExe, true
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK_FALSE(r.ok);
    CHECK(r.error.find("name") != std::string::npos);

    cleanupTree(engineExe);
    cleanupTree(mock.root);
    cleanupTree(destDir);
}

TEST_CASE("PackageBuilder: isDebug=false copia SDL2.dll en lugar de SDL2d.dll") {
    auto engineExe = makeMockEngineExeDir();
    // En el mock pusimos solo SDL2d.dll. Para Release necesitamos SDL2.dll.
    writeFile(engineExe / "SDL2.dll", "fake release dll");
    auto mock    = makeMockProject("release");
    auto destDir = makeTempDir("dest_release");

    PackageBuilder::BuildConfig cfg{
        mock.project, mock.moodprojPath, destDir, engineExe, /*isDebug=*/false
    };
    const auto r = PackageBuilder::build(cfg);

    CHECK(r.ok);
    CHECK(std::filesystem::exists(r.outputDir / "SDL2.dll"));
    CHECK_FALSE(std::filesystem::exists(r.outputDir / "SDL2d.dll"));

    cleanupTree(engineExe);
    cleanupTree(mock.root);
    cleanupTree(destDir);
}
