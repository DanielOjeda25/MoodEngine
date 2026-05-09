#include "engine/scene/serialization/ProjectSerializer.h"

#include "core/Log.h"
#include "engine/scene/serialization/JsonHelpers.h"

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

    // Hito 40 G: char controller settings — solo persistir si != defaults
    // para no ensuciar `.moodproj` viejos con campos nuevos.
    if (project.coyoteWindowSec != 0.10f) {
        j["coyoteWindowSec"] = project.coyoteWindowSec;
    }
    if (project.jumpBufferWindowSec != 0.15f) {
        j["jumpBufferWindowSec"] = project.jumpBufferWindowSec;
    }
    // F2H35 Bloque E: toggle Nombres — solo persistir si != default
    // (true). Mismo patron que los settings de Hito 40 G.
    if (!project.showEntityLabels) {
        j["showEntityLabels"] = project.showEntityLabels;
    }

    // F2H7: workspaces — solo persistir si la lista no esta vacia (proyectos
    // nuevos los inicializan al abrirse via WorkspaceManager defaults).
    if (!project.workspaces.empty()) {
        // F2H35: stamp del schema de iniLayout. Ver JsonHelpers.h.
        j["ini_layout_stamp"] = k_IniLayoutStamp;
        j["workspaces"] = json::array();
        for (const auto& ws : project.workspaces) {
            j["workspaces"].push_back({
                {"name",       ws.name},
                {"ini_layout", ws.iniLayout},
            });
        }
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
        // Hito 40 G: char controller settings opcionales.
        if (j.contains("coyoteWindowSec")) {
            p.coyoteWindowSec = j.at("coyoteWindowSec").get<f32>();
        }
        if (j.contains("jumpBufferWindowSec")) {
            p.jumpBufferWindowSec = j.at("jumpBufferWindowSec").get<f32>();
        }
        // F2H35 Bloque E: toggle Nombres opcional. Ausente = default true.
        if (j.contains("showEntityLabels")) {
            p.showEntityLabels = j.at("showEntityLabels").get<bool>();
        }

        // F2H7: workspaces opcionales. Si no estan, queda vacio y el
        // WorkspaceManager usara los 4 defaults al cargar el proyecto.
        // F2H35: si el stamp del iniLayout no matchea el actual (campo
        // ausente = stamp 0 legacy), descartar los iniLayout y dejar que
        // el editor reconstruya fresh con el WorkSize correcto al
        // primer activado del workspace. Conservamos los nombres para
        // preservar el orden de tabs custom del dev.
        const int storedStamp = j.value("ini_layout_stamp", 0);
        const bool invalidateLayouts = (storedStamp != k_IniLayoutStamp);
        if (invalidateLayouts && j.contains("workspaces")) {
            Log::assets()->info(
                "ProjectSerializer::load: iniLayout stamp {} != actual {}, "
                "descartando layouts persistidos (rebuild fresh al activar)",
                storedStamp, k_IniLayoutStamp);
        }
        if (j.contains("workspaces")) {
            for (const auto& wj : j.at("workspaces")) {
                Workspace ws;
                ws.name = wj.value("name", std::string{});
                ws.iniLayout = invalidateLayouts
                    ? std::string{}
                    : wj.value("ini_layout", std::string{});
                if (!ws.name.empty()) {
                    p.workspaces.push_back(std::move(ws));
                }
            }
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
