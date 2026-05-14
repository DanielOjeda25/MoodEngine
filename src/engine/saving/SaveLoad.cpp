#include "engine/saving/SaveLoad.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace Mood::SaveLoad {

namespace {
constexpr int k_supportedVersion = 3;  // F2H53 H: bump v2 -> v3 (quests)

// Convierte un ExposedValue al primitive JSON correspondiente.
// Reusamos el patron del EntitySerializer (Hito 24) — lo duplicamos
// porque el include cruza modules. Si crece, extraer a JsonHelpers.
nlohmann::json exposedValueToJson(const ExposedValue& v) {
    return std::visit([](auto&& val) -> nlohmann::json {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, glm::vec3>) {
            return nlohmann::json(std::vector<f32>{val.x, val.y, val.z});
        } else {
            return nlohmann::json(val);
        }
    }, v);
}

std::optional<ExposedValue> jsonToExposedValue(const nlohmann::json& jv) {
    if (jv.is_boolean()) return ExposedValue{jv.get<bool>()};
    if (jv.is_number())  return ExposedValue{jv.get<f32>()};
    if (jv.is_string())  return ExposedValue{jv.get<std::string>()};
    if (jv.is_array() && jv.size() == 3) {
        return ExposedValue{glm::vec3(
            jv[0].get<f32>(), jv[1].get<f32>(), jv[2].get<f32>())};
    }
    return std::nullopt;
}
} // namespace

bool save(const SaveData& d, const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    nlohmann::json j;
    j["version"]  = k_supportedVersion;
    j["map_path"] = d.mapPath;
    j["hud"]["hp"]   = d.hud.hp;
    j["hud"]["ammo"] = d.hud.ammo;
    // F2H39: campos nuevos del HUD framework. Solo serializar si difieren
    // del default — keep saves chicas para gameplay basico.
    if (d.hud.max_hp  != 100) j["hud"]["max_hp"]  = d.hud.max_hp;
    if (d.hud.mag     !=  30) j["hud"]["mag"]     = d.hud.mag;
    if (d.hud.max_mag !=  30) j["hud"]["max_mag"] = d.hud.max_mag;
    if (d.hud.reserve !=  90) j["hud"]["reserve"] = d.hud.reserve;
    if (!d.hud.interact_prompt.empty())
        j["hud"]["interact_prompt"] = d.hud.interact_prompt;
    // hit_marker_t / damage_t / pickup_queue son state transient — NO se
    // persisten (no tiene sentido restaurar un hit marker a medio fade
    // tras load).
    j["player"]["position"] = std::vector<f32>{
        d.playerPosition.x, d.playerPosition.y, d.playerPosition.z};
    j["player"]["yaw"]   = d.playerYaw;
    j["player"]["pitch"] = d.playerPitch;

    // Hito 41 A: snapshots de Dynamic bodies.
    if (!d.bodies.empty()) {
        j["bodies"] = nlohmann::json::array();
        for (const auto& b : d.bodies) {
            nlohmann::json jb;
            jb["tag"]               = b.entityTag;
            jb["position"]          = std::vector<f32>{
                b.position.x, b.position.y, b.position.z};
            jb["rotation"]          = std::vector<f32>{
                b.rotationQuat.x, b.rotationQuat.y,
                b.rotationQuat.z, b.rotationQuat.w};
            jb["linear_velocity"]   = std::vector<f32>{
                b.linearVelocity.x, b.linearVelocity.y, b.linearVelocity.z};
            jb["angular_velocity"]  = std::vector<f32>{
                b.angularVelocity.x, b.angularVelocity.y, b.angularVelocity.z};
            j["bodies"].push_back(std::move(jb));
        }
    }

    // Hito 41 B: Lua script globals filtradas.
    if (!d.scriptGlobals.empty()) {
        j["script_globals"] = nlohmann::json::array();
        for (const auto& sg : d.scriptGlobals) {
            nlohmann::json jsg;
            jsg["path"]    = sg.scriptPath;
            jsg["globals"] = nlohmann::json::object();
            for (const auto& [name, value] : sg.globals) {
                jsg["globals"][name] = exposedValueToJson(value);
            }
            j["script_globals"].push_back(std::move(jsg));
        }
    }

    // F2H53 H: quest state runtime — path logico + state enum + bool[]
    // de objective progress. NO serializamos el quest tracked si esta vacio.
    if (!d.quests.empty()) {
        j["quests"] = nlohmann::json::array();
        for (const auto& q : d.quests) {
            nlohmann::json jq;
            jq["path"]            = q.path;
            jq["state"]           = q.state;
            jq["objective_done"]  = q.objectiveDone;  // nlohmann handles vector<bool>
            j["quests"].push_back(std::move(jq));
        }
    }
    if (!d.trackedQuestPath.empty()) {
        j["tracked_quest"] = d.trackedQuestPath;
    }

    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    if (!f.is_open()) {
        Log::engine()->warn(
            "SaveLoad::save: no se pudo abrir '{}' para escritura",
            path.generic_string());
        return false;
    }
    f << j.dump(2);
    Log::engine()->info(
        "SaveLoad::save: '{}' OK ({} bytes, {} bodies, {} script globals, "
        "{} quests, hp={}, ammo={})",
        path.generic_string(),
        static_cast<usize>(f.tellp()),
        d.bodies.size(),
        d.scriptGlobals.size(),
        d.quests.size(),
        d.hud.hp, d.hud.ammo);
    return true;
}

std::optional<SaveData> load(const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    if (!fs::exists(path)) {
        Log::engine()->warn(
            "SaveLoad::load: '{}' no existe", path.generic_string());
        return std::nullopt;
    }
    std::ifstream f(path);
    if (!f.is_open()) {
        Log::engine()->warn(
            "SaveLoad::load: no se pudo abrir '{}' para lectura",
            path.generic_string());
        return std::nullopt;
    }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        Log::engine()->warn(
            "SaveLoad::load: '{}' no parsea como JSON ({})",
            path.generic_string(), e.what());
        return std::nullopt;
    }

    const int v = j.value("version", 0);
    if (v <= 0 || v > k_supportedVersion) {
        Log::engine()->warn(
            "SaveLoad::load: '{}' tiene version {} (soportado: 1..{})",
            path.generic_string(), v, k_supportedVersion);
        return std::nullopt;
    }

    SaveData d;
    d.mapPath = j.value("map_path", std::string{});
    if (j.contains("hud")) {
        const auto& jhud = j.at("hud");
        d.hud.hp   = jhud.value("hp",   d.hud.hp);
        d.hud.ammo = jhud.value("ammo", d.hud.ammo);
        // F2H39: campos nuevos opcionales. Saves pre-F2H39 no los tienen
        // - default values en HudState garantizan back-compat.
        d.hud.max_hp           = jhud.value("max_hp",  d.hud.max_hp);
        d.hud.mag              = jhud.value("mag",     d.hud.mag);
        d.hud.max_mag          = jhud.value("max_mag", d.hud.max_mag);
        d.hud.reserve          = jhud.value("reserve", d.hud.reserve);
        d.hud.interact_prompt  = jhud.value("interact_prompt",
                                             d.hud.interact_prompt);
    }
    if (j.contains("player")) {
        const auto& jp = j.at("player");
        if (jp.contains("position") && jp.at("position").is_array()
            && jp.at("position").size() == 3) {
            d.playerPosition = glm::vec3(
                jp.at("position")[0].get<f32>(),
                jp.at("position")[1].get<f32>(),
                jp.at("position")[2].get<f32>());
        }
        d.playerYaw   = jp.value("yaw",   d.playerYaw);
        d.playerPitch = jp.value("pitch", d.playerPitch);
    }

    // Hito 41 A: bodies array (opcional — v1 no lo tiene).
    if (j.contains("bodies") && j.at("bodies").is_array()) {
        for (const auto& jb : j.at("bodies")) {
            BodySnapshot b;
            b.entityTag = jb.value("tag", std::string{});
            if (jb.contains("position") && jb.at("position").is_array()
                && jb.at("position").size() == 3) {
                b.position = glm::vec3(
                    jb.at("position")[0].get<f32>(),
                    jb.at("position")[1].get<f32>(),
                    jb.at("position")[2].get<f32>());
            }
            if (jb.contains("rotation") && jb.at("rotation").is_array()
                && jb.at("rotation").size() == 4) {
                b.rotationQuat = glm::vec4(
                    jb.at("rotation")[0].get<f32>(),
                    jb.at("rotation")[1].get<f32>(),
                    jb.at("rotation")[2].get<f32>(),
                    jb.at("rotation")[3].get<f32>());
            }
            if (jb.contains("linear_velocity") && jb.at("linear_velocity").is_array()
                && jb.at("linear_velocity").size() == 3) {
                b.linearVelocity = glm::vec3(
                    jb.at("linear_velocity")[0].get<f32>(),
                    jb.at("linear_velocity")[1].get<f32>(),
                    jb.at("linear_velocity")[2].get<f32>());
            }
            if (jb.contains("angular_velocity") && jb.at("angular_velocity").is_array()
                && jb.at("angular_velocity").size() == 3) {
                b.angularVelocity = glm::vec3(
                    jb.at("angular_velocity")[0].get<f32>(),
                    jb.at("angular_velocity")[1].get<f32>(),
                    jb.at("angular_velocity")[2].get<f32>());
            }
            if (!b.entityTag.empty()) d.bodies.push_back(std::move(b));
        }
    }

    // F2H53 H: quests array (opcional — v1/v2 no lo tienen).
    if (j.contains("quests") && j.at("quests").is_array()) {
        for (const auto& jq : j.at("quests")) {
            QuestSnapshot q;
            q.path  = jq.value("path", std::string{});
            q.state = jq.value("state", 0);
            if (jq.contains("objective_done") && jq.at("objective_done").is_array()) {
                for (const auto& jb : jq.at("objective_done")) {
                    q.objectiveDone.push_back(jb.get<bool>());
                }
            }
            if (!q.path.empty()) {
                d.quests.push_back(std::move(q));
            }
        }
    }
    d.trackedQuestPath = j.value("tracked_quest", std::string{});

    // Hito 41 B: script_globals array (opcional).
    if (j.contains("script_globals") && j.at("script_globals").is_array()) {
        for (const auto& jsg : j.at("script_globals")) {
            ScriptGlobalsSnapshot sg;
            sg.scriptPath = jsg.value("path", std::string{});
            if (jsg.contains("globals") && jsg.at("globals").is_object()) {
                for (const auto& item : jsg.at("globals").items()) {
                    if (auto opt = jsonToExposedValue(item.value());
                        opt.has_value()) {
                        sg.globals[item.key()] = std::move(*opt);
                    }
                }
            }
            if (!sg.scriptPath.empty()) {
                d.scriptGlobals.push_back(std::move(sg));
            }
        }
    }

    Log::engine()->info(
        "SaveLoad::load: '{}' OK (map='{}', hp={}, ammo={}, {} bodies, "
        "{} script globals, {} quests)",
        path.generic_string(),
        d.mapPath, d.hud.hp, d.hud.ammo,
        d.bodies.size(), d.scriptGlobals.size(), d.quests.size());
    return d;
}

} // namespace Mood::SaveLoad
