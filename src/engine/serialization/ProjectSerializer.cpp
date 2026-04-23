#include "engine/serialization/ProjectSerializer.h"

#include "core/Log.h"
#include "engine/serialization/JsonHelpers.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <stdexcept>

namespace Mood {

using json = nlohmann::json;

namespace {

std::filesystem::path moodprojFile(const Project& p) {
    return p.root / (p.name + ".moodproj");
}

} // namespace

void ProjectSerializer::save(const Project& project) {
    if (project.root.empty()) {
        throw std::runtime_error("ProjectSerializer::save: Project::root vacio");
    }
    if (project.name.empty()) {
        throw std::runtime_error("ProjectSerializer::save: Project::name vacio");
    }

    json j;
    j["version"]    = k_MoodprojFormatVersion;
    j["name"]       = project.name;
    j["defaultMap"] = project.defaultMap.generic_string();

    j["maps"] = json::array();
    for (const auto& m : project.maps) {
        j["maps"].push_back(m.generic_string());
    }

    const auto path = moodprojFile(project);
    std::error_code ec;
    std::filesystem::create_directories(project.root, ec);

    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("ProjectSerializer::save: no se pudo abrir '" +
                                 path.generic_string() + "' para escritura");
    }
    out << j.dump(2);
    Log::assets()->info("Proyecto guardado: {}", path.generic_string());
}

std::optional<Project> ProjectSerializer::load(const std::filesystem::path& moodprojPath) {
    std::ifstream in(moodprojPath);
    if (!in.is_open()) {
        Log::assets()->warn("ProjectSerializer::load: no se pudo abrir '{}'",
                            moodprojPath.generic_string());
        return std::nullopt;
    }

    json j;
    try {
        in >> j;
    } catch (const std::exception& e) {
        Log::assets()->warn("ProjectSerializer::load: JSON invalido en '{}': {}",
                            moodprojPath.generic_string(), e.what());
        return std::nullopt;
    }

    try {
        checkFormatVersion(j, k_MoodprojFormatVersion, "moodproj");

        Project p;
        p.root       = moodprojPath.parent_path();
        p.name       = j.at("name").get<std::string>();
        p.defaultMap = std::filesystem::path(j.at("defaultMap").get<std::string>());
        for (const auto& mj : j.at("maps")) {
            p.maps.emplace_back(mj.get<std::string>());
        }

        Log::assets()->info("Proyecto cargado: {} ({} mapas)",
                            moodprojPath.generic_string(), p.maps.size());
        return p;
    } catch (const std::exception& e) {
        Log::assets()->warn("ProjectSerializer::load: falla semantica en '{}': {}",
                            moodprojPath.generic_string(), e.what());
        return std::nullopt;
    }
}

std::optional<Project> ProjectSerializer::createNewProject(
        const std::filesystem::path& root,
        const std::string& name,
        const std::string& defaultMapName) {
    if (name.empty()) {
        Log::assets()->warn("createNewProject: name vacio");
        return std::nullopt;
    }

    std::error_code ec;
    std::filesystem::create_directories(root / "maps", ec);
    if (ec) {
        Log::assets()->warn("createNewProject: no pude crear '{}/maps': {}",
                            root.generic_string(), ec.message());
        return std::nullopt;
    }
    std::filesystem::create_directories(root / "textures", ec);
    if (ec) {
        Log::assets()->warn("createNewProject: no pude crear '{}/textures': {}",
                            root.generic_string(), ec.message());
        return std::nullopt;
    }

    // Si no se especifico nombre de mapa, usar el del proyecto. Evita que
    // dos proyectos en la misma raiz compartan "maps/default.moodmap" y se
    // clobbereen mutuamente al guardar.
    const std::string mapName = defaultMapName.empty() ? name : defaultMapName;

    Project p;
    p.root       = root;
    p.name       = name;
    p.defaultMap = std::filesystem::path("maps") / (mapName + ".moodmap");
    p.maps       = {p.defaultMap};

    // El `.moodproj` se escribe ahora; el `.moodmap` default queda a cargo
    // del caller (tiene que pasar el estado actual del editor por
    // SceneSerializer::save). Esto evita acoplar este modulo a GridMap.
    try {
        save(p);
    } catch (const std::exception& e) {
        Log::assets()->warn("createNewProject: save fallo: {}", e.what());
        return std::nullopt;
    }
    return p;
}

} // namespace Mood
