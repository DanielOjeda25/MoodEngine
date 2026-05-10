// Tests de Mood::I18n (F2H43): loader JSON, lookup con fallback Ingles,
// switch de idioma, interpolacion fmt, key faltante.
//
// I18n es proceso-global (mismo patron que Log/GameState). Cada test
// inicializa con su propio dir temporal y llama shutdown al final, asi
// no filtran estado entre tests si doctest los paraleliza/shuffleia.

#include <doctest/doctest.h>

#include "core/UserSettings.h"
#include "engine/i18n/I18n.h"

#include <filesystem>
#include <fstream>
#include <string>

using namespace Mood;
namespace fs = std::filesystem;

namespace {

// Crea un dir temp con `en.json` + `es.json` y devuelve el path. El
// caller se encarga de borrarlo al final.
fs::path makeTempI18nDir(const std::string& slug) {
    const fs::path dir = fs::temp_directory_path() / ("mood_i18n_" + slug);
    fs::remove_all(dir);
    fs::create_directories(dir);
    {
        std::ofstream out(dir / "en.json");
        out << R"({
            "_comment": "test fixture EN",
            "menu.file": "File",
            "menu.help": "Help",
            "msg.greet": "Hello, {}!",
            "only_in_en": "ONLY EN"
        })";
    }
    {
        std::ofstream out(dir / "es.json");
        out << R"({
            "_comment": "test fixture ES",
            "menu.file": "Archivo",
            "menu.help": "Ayuda",
            "msg.greet": "Hola, {}!"
        })";
    }
    return dir;
}

void cleanup(const fs::path& dir) {
    fs::remove_all(dir);
}

} // namespace

TEST_CASE("I18n init carga ambos JSON y queda activo el default") {
    const auto dir = makeTempI18nDir("init");
    I18n::init(I18n::Language::Spanish, dir.string());
    CHECK(I18n::currentLanguage() == I18n::Language::Spanish);
    CHECK(I18n::T("menu.file") == "Archivo");
    CHECK(I18n::T("menu.help") == "Ayuda");
    I18n::shutdown();
    cleanup(dir);
}

TEST_CASE("I18n key faltante en activo cae a fallback Ingles") {
    const auto dir = makeTempI18nDir("fallback");
    I18n::init(I18n::Language::Spanish, dir.string());
    // `only_in_en` no existe en es.json, debe caer a en.json.
    CHECK(I18n::T("only_in_en") == "ONLY EN");
    I18n::shutdown();
    cleanup(dir);
}

TEST_CASE("I18n key inexistente devuelve la key cruda") {
    const auto dir = makeTempI18nDir("missing");
    I18n::init(I18n::Language::Spanish, dir.string());
    CHECK(I18n::T("does.not.exist") == "does.not.exist");
    I18n::shutdown();
    cleanup(dir);
}

TEST_CASE("I18n setLanguage cambia activo y traducciones") {
    const auto dir = makeTempI18nDir("switch");
    I18n::init(I18n::Language::Spanish, dir.string());
    CHECK(I18n::T("menu.file") == "Archivo");
    REQUIRE(I18n::setLanguage(I18n::Language::English));
    CHECK(I18n::currentLanguage() == I18n::Language::English);
    CHECK(I18n::T("menu.file") == "File");
    REQUIRE(I18n::setLanguage(I18n::Language::Spanish));
    CHECK(I18n::T("menu.file") == "Archivo");
    I18n::shutdown();
    cleanup(dir);
}

TEST_CASE("I18n T con interpolacion estilo fmt") {
    const auto dir = makeTempI18nDir("fmt");
    I18n::init(I18n::Language::Spanish, dir.string());
    CHECK(I18n::T("msg.greet", "Daniel") == "Hola, Daniel!");
    REQUIRE(I18n::setLanguage(I18n::Language::English));
    CHECK(I18n::T("msg.greet", "Daniel") == "Hello, Daniel!");
    I18n::shutdown();
    cleanup(dir);
}

TEST_CASE("UserSettings save/load roundtrip via APPDATA") {
    // No podemos tocar APPDATA real; pero sí podemos verificar que
    // settingsPath() devuelve una ruta no vacia que termina en
    // settings.json — eso confirma el path resolver.
    const auto path = Mood::UserSettings::settingsPath();
    CHECK(path.filename() == "settings.json");
}

TEST_CASE("I18n languageCode y languageFromCode roundtrip") {
    CHECK(std::string(I18n::languageCode(I18n::Language::English)) == "en");
    CHECK(std::string(I18n::languageCode(I18n::Language::Spanish)) == "es");
    CHECK(I18n::languageFromCode("en") == I18n::Language::English);
    CHECK(I18n::languageFromCode("es") == I18n::Language::Spanish);
    // Codigo desconocido cae a English.
    CHECK(I18n::languageFromCode("zh") == I18n::Language::English);
}
