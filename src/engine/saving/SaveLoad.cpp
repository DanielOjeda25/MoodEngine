#include "engine/saving/SaveLoad.h"

#include "core/Log.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace Mood::SaveLoad {

namespace {
constexpr int k_supportedVersion = 1;
} // namespace

bool save(const SaveData& d, const std::filesystem::path& path) {
    namespace fs = std::filesystem;
    nlohmann::json j;
    j["version"]  = k_supportedVersion;
    j["map_path"] = d.mapPath;
    j["hud"]["hp"]   = d.hud.hp;
    j["hud"]["ammo"] = d.hud.ammo;
    j["player"]["position"] = std::vector<f32>{
        d.playerPosition.x, d.playerPosition.y, d.playerPosition.z};
    j["player"]["yaw"]   = d.playerYaw;
    j["player"]["pitch"] = d.playerPitch;

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
    Log::engine()->info("SaveLoad::save: '{}' OK ({} bytes)",
                         path.generic_string(),
                         static_cast<usize>(f.tellp()));
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

    Log::engine()->info("SaveLoad::load: '{}' OK (map='{}', hp={}, ammo={})",
                         path.generic_string(),
                         d.mapPath, d.hud.hp, d.hud.ammo);
    return d;
}

} // namespace Mood::SaveLoad
