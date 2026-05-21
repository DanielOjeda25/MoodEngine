#include "core/UserSettings.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>

namespace Mood::UserSettings {

namespace {

I18n::Language s_language = I18n::Language::Spanish;
std::string s_theme = "dark";  // F2H76: default dark
std::filesystem::path s_path;

std::filesystem::path computePath() {
    // Windows: %APPDATA% es la convencion (`C:\Users\<name>\AppData\Roaming`).
    // Si no existe la env var (caso raro), cae al cwd.
    const char* appdata = std::getenv("APPDATA");
    if (appdata != nullptr && *appdata != '\0') {
        return std::filesystem::path(appdata) / "MoodEngine" / "settings.json";
    }
    return std::filesystem::current_path() / "moodengine_settings.json";
}

} // namespace

void init() {
    s_path = computePath();
    s_language = I18n::Language::Spanish;  // default si no hay archivo
    s_theme = "dark";

    std::ifstream in(s_path);
    if (!in.is_open()) {
        Log::engine()->info("[settings] no existe '{}' — usando defaults",
                              s_path.generic_string());
        return;
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->warn("[settings] parse error en '{}': {} — usando defaults",
                              s_path.generic_string(), e.what());
        return;
    }

    if (j.contains("language") && j["language"].is_string()) {
        s_language = I18n::languageFromCode(j["language"].get<std::string>());
    }
    if (j.contains("theme") && j["theme"].is_string()) {
        s_theme = j["theme"].get<std::string>();
    }
    Log::engine()->info("[settings] cargado '{}' (language={}, theme={})",
                         s_path.generic_string(),
                         I18n::languageCode(s_language), s_theme);
}

void shutdown() {
    // No-op por ahora; placeholder para simetria con init().
}

bool save() {
    if (s_path.empty()) s_path = computePath();
    std::error_code ec;
    std::filesystem::create_directories(s_path.parent_path(), ec);
    if (ec) {
        Log::engine()->warn("[settings] no se pudo crear dir '{}': {}",
                              s_path.parent_path().generic_string(), ec.message());
        return false;
    }
    std::ofstream out(s_path);
    if (!out.is_open()) {
        Log::engine()->warn("[settings] no se pudo abrir '{}' para escritura",
                              s_path.generic_string());
        return false;
    }
    nlohmann::json j;
    j["language"] = I18n::languageCode(s_language);
    j["theme"]    = s_theme;
    out << j.dump(2) << "\n";
    Log::engine()->info("[settings] guardado '{}' (language={}, theme={})",
                         s_path.generic_string(),
                         I18n::languageCode(s_language), s_theme);
    return true;
}

I18n::Language language() { return s_language; }

void setLanguage(I18n::Language lang) { s_language = lang; }

const std::string& theme() { return s_theme; }

void setTheme(const std::string& id) { s_theme = id; }

std::filesystem::path settingsPath() {
    if (s_path.empty()) return computePath();
    return s_path;
}

} // namespace Mood::UserSettings
