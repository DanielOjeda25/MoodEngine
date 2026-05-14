#include "engine/quest/QuestAsset.h"

#include "core/Log.h"

#include <fstream>

namespace Mood::Quest {

// =============================================================
// Helpers de enums <-> string
// =============================================================

const char* objectiveTypeToString(ObjectiveType t) {
    switch (t) {
        case ObjectiveType::Collect:   return "collect";
        case ObjectiveType::Talk:      return "talk";
        case ObjectiveType::Reach:     return "reach";
        case ObjectiveType::CustomLua: return "custom_lua";
    }
    return "custom_lua";
}

ObjectiveType objectiveTypeFromString(const std::string& s) {
    if (s == "collect")    return ObjectiveType::Collect;
    if (s == "talk")       return ObjectiveType::Talk;
    if (s == "reach")      return ObjectiveType::Reach;
    // Default + unknown -> CustomLua. Custom_predicate vacio queda como
    // "objective siempre incompleto" (eval Lua de "" -> falsy), lo que es
    // el comportamiento defensivo deseado para datos invalidos.
    return ObjectiveType::CustomLua;
}

const char* rewardTypeToString(RewardType t) {
    switch (t) {
        case RewardType::Item: return "item";
        case RewardType::Var:  return "var";
        case RewardType::Lua:  return "lua";
    }
    return "lua";
}

RewardType rewardTypeFromString(const std::string& s) {
    if (s == "item") return RewardType::Item;
    if (s == "var")  return RewardType::Var;
    // Default + unknown -> Lua. Script vacio no muta nada, comportamiento
    // defensivo igual al de objectives.
    return RewardType::Lua;
}

// =============================================================
// Serializacion — Objective
// =============================================================

namespace {

nlohmann::json objectiveToJson(const Objective& o) {
    // Serializamos siempre TODOS los campos type-specific, aunque algunos
    // sean ignorados por el `type` actual. Ventaja: el dev puede cambiar el
    // type en el editor sin perder los valores previos. Costo: ~50 bytes
    // extra por objective en disco, despreciable.
    return nlohmann::json{
        {"id",                  o.id},
        {"name_key",            o.name_key},
        {"name_literal",        o.name_literal},
        {"description_literal", o.description_literal},
        {"type",                objectiveTypeToString(o.type)},
        {"item_path",           o.item_path},
        {"min_quantity",        o.min_quantity},
        {"var_name",            o.var_name},
        {"custom_predicate",    o.custom_predicate},
    };
}

Objective objectiveFromJson(const nlohmann::json& j) {
    Objective o;
    if (!j.is_object()) return o;

    o.id                  = j.value("id",                  std::string{});
    o.name_key            = j.value("name_key",            std::string{});
    o.name_literal        = j.value("name_literal",        std::string{});
    o.description_literal = j.value("description_literal", std::string{});
    o.type                = objectiveTypeFromString(
                                j.value("type", std::string{"custom_lua"}));
    o.item_path           = j.value("item_path",           std::string{});
    o.min_quantity        = j.value("min_quantity",        1);
    o.var_name            = j.value("var_name",            std::string{});
    o.custom_predicate    = j.value("custom_predicate",    std::string{});
    return o;
}

// =============================================================
// Serializacion — Reward
// =============================================================

nlohmann::json rewardToJson(const Reward& r) {
    return nlohmann::json{
        {"type",       rewardTypeToString(r.type)},
        {"item_path",  r.item_path},
        {"quantity",   r.quantity},
        {"var_name",   r.var_name},
        {"var_value",  r.var_value},
        {"lua_script", r.lua_script},
    };
}

Reward rewardFromJson(const nlohmann::json& j) {
    Reward r;
    if (!j.is_object()) return r;

    r.type       = rewardTypeFromString(j.value("type", std::string{"lua"}));
    r.item_path  = j.value("item_path",  std::string{});
    r.quantity   = j.value("quantity",   1);
    r.var_name   = j.value("var_name",   std::string{});
    r.var_value  = j.value("var_value",  std::string{});
    r.lua_script = j.value("lua_script", std::string{});
    return r;
}

} // namespace

// =============================================================
// Serializacion — Asset
// =============================================================

nlohmann::json Asset::toJson() const {
    nlohmann::json objArr = nlohmann::json::array();
    for (const Objective& o : objectives) {
        objArr.push_back(objectiveToJson(o));
    }

    nlohmann::json rewArr = nlohmann::json::array();
    for (const Reward& r : rewards) {
        rewArr.push_back(rewardToJson(r));
    }

    return nlohmann::json{
        {"_version",            k_schemaVersion},
        {"id",                  id},
        {"name_key",            name_key},
        {"name_literal",        name_literal},
        {"description_key",     description_key},
        {"description_literal", description_literal},
        {"category",            category},
        {"objectives",          std::move(objArr)},
        {"rewards",             std::move(rewArr)},
    };
}

Asset Asset::fromJson(const nlohmann::json& j) {
    Asset a;
    if (!j.is_object()) {
        Log::engine()->error("[QuestAsset] fromJson: JSON no es objeto");
        return a;
    }
    const u32 version = j.value("_version", 0u);
    if (version != k_schemaVersion) {
        Log::engine()->error(
            "[QuestAsset] fromJson: schema version {} != esperado {}",
            version, k_schemaVersion);
        return a;
    }

    a.id                  = j.value("id",                  std::string{});
    a.name_key            = j.value("name_key",            std::string{});
    a.name_literal        = j.value("name_literal",        std::string{});
    a.description_key     = j.value("description_key",     std::string{});
    a.description_literal = j.value("description_literal", std::string{});
    a.category            = j.value("category",            std::string{});

    if (j.contains("objectives") && j["objectives"].is_array()) {
        for (const auto& jo : j["objectives"]) {
            a.objectives.push_back(objectiveFromJson(jo));
        }
    }

    if (j.contains("rewards") && j["rewards"].is_array()) {
        for (const auto& jr : j["rewards"]) {
            a.rewards.push_back(rewardFromJson(jr));
        }
    }

    return a;
}

// =============================================================
// I/O de disco
// =============================================================

std::optional<Asset> Asset::loadFromFile(const std::filesystem::path& fsPath) {
    std::ifstream in(fsPath);
    if (!in.is_open()) {
        Log::engine()->warn("[QuestAsset] no se pudo abrir '{}'",
                             fsPath.generic_string());
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::engine()->error("[QuestAsset] parse error en '{}': {}",
                              fsPath.generic_string(), e.what());
        return std::nullopt;
    }
    Asset a = fromJson(j);
    // Mismo fallback que ItemAsset: si el JSON no traia `id`, usar el
    // filename (sin extension). El editor garantiza la convencion al
    // crear/renombrar.
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
        Log::engine()->error("[QuestAsset] no se pudo abrir '{}' para escritura",
                              fsPath.generic_string());
        return false;
    }
    try {
        out << toJson().dump(2);
    } catch (const std::exception& e) {
        Log::engine()->error("[QuestAsset] error al escribir '{}': {}",
                              fsPath.generic_string(), e.what());
        return false;
    }
    return true;
}

} // namespace Mood::Quest
