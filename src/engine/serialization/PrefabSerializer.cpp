#include "engine/serialization/PrefabSerializer.h"

#include "core/Log.h"
#include "engine/scene/Entity.h"
#include "engine/serialization/EntitySerializer.h"
#include "engine/serialization/JsonHelpers.h" // checkFormatVersion

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace Mood {

using json = nlohmann::json;

void PrefabSerializer::save(Entity entity, const std::string& name,
                             const AssetManager& assets,
                             const std::filesystem::path& path) {
    json j;
    j["version"]  = k_MoodprefabFormatVersion;
    if (!name.empty()) {
        j["name"] = name;
    }
    j["root"]     = serializeEntityToJson(entity, assets);
    j["children"] = json::array();

    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("PrefabSerializer::save: no se pudo abrir '" +
                                 path.generic_string() + "' para escritura");
    }
    out << j.dump(2);
    Log::assets()->info("Prefab guardado: {}", path.generic_string());
}

std::optional<SavedPrefab> PrefabSerializer::load(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        Log::assets()->warn(
            "PrefabSerializer::load: no se pudo abrir '{}'", path.generic_string());
        return std::nullopt;
    }

    json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "PrefabSerializer::load: JSON invalido en '{}': {}",
            path.generic_string(), e.what());
        return std::nullopt;
    }

    try {
        checkFormatVersion(j, k_MoodprefabFormatVersion, "moodprefab");

        SavedPrefab sp;
        sp.name = j.value("name", path.stem().generic_string());
        if (!j.contains("root")) {
            throw std::runtime_error("falta campo 'root'");
        }
        sp.root = parseEntityFromJson(j.at("root"));
        if (j.contains("children") && j.at("children").is_array()) {
            for (const auto& jc : j.at("children")) {
                sp.children.push_back(parseEntityFromJson(jc));
            }
        }
        Log::assets()->info("Prefab cargado: {} ({} children)",
                             path.generic_string(), sp.children.size());
        return sp;
    } catch (const std::exception& e) {
        Log::assets()->warn(
            "PrefabSerializer::load: falla semantica en '{}': {}",
            path.generic_string(), e.what());
        return std::nullopt;
    }
}

} // namespace Mood
