#include "engine/inventory/ItemAsset.h"

#include "core/Log.h"

#include <fstream>

namespace Mood::Inventory {

// =============================================================
// Serializacion
// =============================================================

nlohmann::json Asset::toJson() const {
    nlohmann::json tagsArr = nlohmann::json::array();
    for (const std::string& t : tags) {
        tagsArr.push_back(t);
    }

    nlohmann::json statsObj = nlohmann::json::object();
    for (const auto& [k, v] : stats) {
        statsObj[k] = v;
    }

    return nlohmann::json{
        {"_version",            k_schemaVersion},
        {"id",                  id},
        {"name_key",            name_key},
        {"name_literal",        name_literal},
        {"description_key",     description_key},
        {"description_literal", description_literal},
        {"icon_path",           icon_path},
        {"model_path",          model_path},
        {"tags",                std::move(tagsArr)},
        {"stats",               std::move(statsObj)},
        {"stack", nlohmann::json{
            {"stackable", stack.stackable},
            {"max_stack", stack.max_stack},
        }},
        {"slot_size", nlohmann::json{
            {"width",  slot_size.width},
            {"height", slot_size.height},
        }},
    };
}

Asset Asset::fromJson(const nlohmann::json& j) {
    Asset a;
    if (!j.is_object()) {
        Log::engine()->error("[ItemAsset] fromJson: JSON no es objeto");
        return a;
    }
    const u32 version = j.value("_version", 0u);
    if (version != k_schemaVersion) {
        Log::engine()->error(
            "[ItemAsset] fromJson: schema version {} != esperado {}",
            version, k_schemaVersion);
        return a;
    }

    a.id                  = j.value("id",                  std::string{});
    a.name_key            = j.value("name_key",            std::string{});
    a.name_literal        = j.value("name_literal",        std::string{});
    a.description_key     = j.value("description_key",     std::string{});
    a.description_literal = j.value("description_literal", std::string{});
    a.icon_path           = j.value("icon_path",           std::string{});
    a.model_path          = j.value("model_path",          std::string{});

    if (j.contains("tags") && j["tags"].is_array()) {
        for (const auto& t : j["tags"]) {
            if (t.is_string()) a.tags.push_back(t.get<std::string>());
        }
    }

    if (j.contains("stats") && j["stats"].is_object()) {
        for (auto it = j["stats"].begin(); it != j["stats"].end(); ++it) {
            if (it.value().is_number()) {
                a.stats[it.key()] = it.value().get<float>();
            }
        }
    }

    if (j.contains("stack") && j["stack"].is_object()) {
        const auto& s = j["stack"];
        a.stack.stackable = s.value("stackable", false);
        a.stack.max_stack = s.value("max_stack", 1);
    }

    if (j.contains("slot_size") && j["slot_size"].is_object()) {
        const auto& s = j["slot_size"];
        a.slot_size.width  = s.value("width",  1);
        a.slot_size.height = s.value("height", 1);
    }

    return a;
}

// =============================================================
// I/O de disco
// =============================================================

std::optional<Asset> Asset::loadFromFile(const std::filesystem::path& fsPath) {
    std::ifstream in(fsPath);
    if (!in.is_open()) {
        Log::engine()->warn("[ItemAsset] no se pudo abrir '{}'", fsPath.generic_string());
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->error("[ItemAsset] parse error en '{}': {}",
                              fsPath.generic_string(), e.what());
        return std::nullopt;
    }
    Asset a = fromJson(j);
    // Si el JSON no traia id pero el archivo si tiene stem (filename
    // sin extension), usar el stem como id. Mantiene consistencia
    // con la convencion del Item Browser.
    if (a.id.empty()) {
        a.id = fsPath.stem().string();
    }
    return a;
}

bool Asset::saveToFile(const std::filesystem::path& fsPath) const {
    std::error_code ec;
    std::filesystem::create_directories(fsPath.parent_path(), ec);
    std::ofstream out(fsPath);
    if (!out.is_open()) {
        Log::engine()->error("[ItemAsset] no se pudo abrir '{}' para escritura",
                              fsPath.generic_string());
        return false;
    }
    try {
        out << toJson().dump(2);
    } catch (const std::exception& e) {
        Log::engine()->error("[ItemAsset] error al escribir '{}': {}",
                              fsPath.generic_string(), e.what());
        return false;
    }
    return true;
}

} // namespace Mood::Inventory
