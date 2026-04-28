#include "engine/game/GameManifest.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace Mood {

std::optional<GameManifest>
GameManifest::loadFromFile(const std::filesystem::path& manifestPath) {
    if (!std::filesystem::exists(manifestPath)) {
        return std::nullopt;
    }

    std::ifstream f(manifestPath);
    if (!f.is_open()) {
        Log::engine()->warn("GameManifest: no se pudo abrir '{}'",
                             manifestPath.generic_string());
        return std::nullopt;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        Log::engine()->warn("GameManifest: JSON invalido en '{}': {}",
                             manifestPath.generic_string(), e.what());
        return std::nullopt;
    }

    GameManifest m{};
    m.version = j.value("version", 1);
    if (m.version != 1) {
        Log::engine()->warn("GameManifest: version {} no soportada en '{}' (esperado 1)",
                             m.version, manifestPath.generic_string());
        return std::nullopt;
    }
    m.name               = j.value("name", std::string{});
    m.projectRelative    = j.value("project", std::string{});
    m.defaultMapRelative = j.value("default_map", std::string{});

    if (m.projectRelative.empty()) {
        Log::engine()->warn(
            "GameManifest: campo 'project' vacio o ausente en '{}'",
            manifestPath.generic_string());
        return std::nullopt;
    }

    return m;
}

} // namespace Mood
