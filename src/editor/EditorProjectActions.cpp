// Implementacion de los handlers del menu Archivo + carga/persistencia
// del proyecto activo (Hito 16 refactor — extraido de EditorApplication.cpp).
//
// Aca viven los handlers que coordinan pfd::dialog, ProjectSerializer y
// SceneSerializer:
//   - confirmDiscardChanges (popup Si/No/Cancelar)
//   - handleNewProject / handleOpenProject / tryOpenProjectPath
//   - handleSave / handleSaveAs / handleCloseProject
//   - addToRecentProjects (dedup + limite)
//   - loadEditorState / saveEditorState (.mood/editor_state.json)
//
// Todos son metodos de `EditorApplication`; aca solo se mueven las
// definiciones para que el .cpp principal no acumule la pipeline completa
// de proyecto.

#include "editor/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/AssetManager.h"
#include "engine/packaging/PackageBuilder.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/serialization/SceneLoader.h"
#include "engine/scene/components/Components.h"

#include <SDL.h>
#include "engine/scene/Entity.h"
#include "engine/scene/Scene.h"
#include "engine/serialization/ProjectSerializer.h"
#include "engine/serialization/SceneSerializer.h"

#include <nlohmann/json.hpp>
#include <portable-file-dialogs.h>

#include <algorithm>
#include <fstream>

namespace Mood {

bool EditorApplication::confirmDiscardChanges() {
    if (!m_projectDirty) return true;

    const auto choice = pfd::message(
        "MoodEngine — Cambios sin guardar",
        "Hay cambios sin guardar en el proyecto actual.\n\n"
        "Si - guardar antes de continuar\n"
        "No - descartar los cambios\n"
        "Cancelar - volver al editor",
        pfd::choice::yes_no_cancel,
        pfd::icon::question).result();

    switch (choice) {
        case pfd::button::yes:
            handleSave();
            // Si el save fallo (p.ej. disco lleno), m_projectDirty sigue
            // en true y abortamos la accion.
            return !m_projectDirty;
        case pfd::button::no:
            return true;
        case pfd::button::cancel:
        default:
            return false;
    }
}

void EditorApplication::handleNewProject() {
    // confirmDiscardChanges: sigue aplicando si el usuario ejecuta "Nuevo"
    // sobre un proyecto modificado sin guardar.
    if (!confirmDiscardChanges()) return;

    const auto cwd = std::filesystem::current_path().generic_string();
    const std::string saveTo = pfd::save_file(
        "Nuevo proyecto — nombre y ubicacion del .moodproj",
        cwd,
        {"Proyectos MoodEngine (*.moodproj)", "*.moodproj"},
        pfd::opt::none).result();
    if (saveTo.empty()) {
        Log::editor()->info("Nuevo proyecto: cancelado");
        return;
    }

    std::filesystem::path moodprojPath(saveTo);
    if (moodprojPath.extension() != ".moodproj") {
        moodprojPath += ".moodproj";
    }

    std::filesystem::path root = moodprojPath.parent_path();
    const std::string name = moodprojPath.stem().generic_string();
    if (name.empty() || root.empty()) {
        pfd::message("MoodEngine", "Nombre o carpeta del proyecto invalidos.",
                     pfd::choice::ok, pfd::icon::warning);
        return;
    }

    // Convencion Unity/Godot: cada proyecto vive en su propia carpeta. Si el
    // usuario eligio `<dir>/test.moodproj` SIN estar dentro de una carpeta
    // que ya se llama `test`, creamos `<dir>/test/` y movemos el proyecto
    // ahi. Asi `<dir>/maps`, `<dir>/textures` no contaminan el directorio
    // padre (ej. el Escritorio).
    if (root.filename().generic_string() != name) {
        const std::filesystem::path subRoot = root / name;
        std::error_code ec;
        std::filesystem::create_directories(subRoot, ec);
        if (ec) {
            pfd::message("MoodEngine",
                std::string("No se pudo crear la carpeta del proyecto:\n") +
                subRoot.generic_string() + "\n" + ec.message(),
                pfd::choice::ok, pfd::icon::error);
            return;
        }
        root = subRoot;
        moodprojPath = subRoot / (name + ".moodproj");
        Log::editor()->info(
            "Nuevo proyecto: carpeta creada en '{}' (DWIM: el .moodproj y sus "
            "subcarpetas maps/, textures/ viven adentro).",
            subRoot.generic_string());
    }

    auto created = ProjectSerializer::createNewProject(root, name);
    if (!created.has_value()) {
        pfd::message("MoodEngine", "No se pudo crear el proyecto (ver console/log).",
                     pfd::choice::ok, pfd::icon::error);
        return;
    }

    // Nuevo proyecto -> contenido inicial = mapa de prueba (template).
    buildInitialTestMap();
    rebuildSceneFromMap();

    const auto mapPath = created->root / created->defaultMap;
    std::filesystem::create_directories(mapPath.parent_path());
    SceneSerializer::save(m_map, created->name, m_scene.get(), *m_assetManager, mapPath);

    m_project = std::move(created);
    m_currentMapPath = m_project->defaultMap;
    m_projectDirty = false;
    addToRecentProjects(m_project->root / (m_project->name + ".moodproj"));
    updateWindowTitle();
    m_ui.setStatusMessage("Proyecto creado: " + m_project->name);
    Log::editor()->info("Proyecto creado: {} ({})",
        m_project->name,
        (m_project->root / (m_project->name + ".moodproj")).generic_string());
}

void EditorApplication::handleOpenProject() {
    if (!confirmDiscardChanges()) return;

    const auto cwd = std::filesystem::current_path().generic_string();
    const auto selection = pfd::open_file(
        "Abrir proyecto", cwd,
        {"Proyectos MoodEngine (*.moodproj)", "*.moodproj"},
        pfd::opt::none).result();
    if (selection.empty()) {
        Log::editor()->info("Abrir proyecto: cancelado");
        return;
    }

    if (!tryOpenProjectPath(std::filesystem::path(selection.front()))) {
        pfd::message("MoodEngine", "No se pudo abrir el proyecto (ver console/log).",
                     pfd::choice::ok, pfd::icon::error);
    }
}

bool EditorApplication::tryOpenProjectPath(const std::filesystem::path& moodproj) {
    if (!std::filesystem::exists(moodproj)) {
        Log::editor()->warn("Proyecto no existe en disco: {}", moodproj.generic_string());
        return false;
    }
    auto loaded = ProjectSerializer::load(moodproj);
    if (!loaded.has_value() || loaded->maps.empty()) {
        Log::editor()->warn("Proyecto invalido o sin mapas: {}", moodproj.generic_string());
        return false;
    }

    const auto mapPath = loaded->root / loaded->defaultMap;
    auto savedMap = SceneSerializer::load(mapPath, *m_assetManager);
    if (!savedMap.has_value()) {
        Log::editor()->warn("No se pudo cargar mapa default: {}", mapPath.generic_string());
        return false;
    }

    m_map = std::move(savedMap->map);
    rebuildSceneFromMap();
    // Entidades no-tile persistidas. Hito 21 Bloque 4: extraido a
    // `engine/serialization/SceneLoader::applyEntitiesToScene` para
    // compartirlo con MoodPlayer.
    if (m_scene) {
        SceneLoader::applyEntitiesToScene(*savedMap, *m_scene, *m_assetManager);
    }
    // Hito 15: aplicar el Environment del proyecto recien cargado YA, en
    // lugar de esperar al primer renderSceneToViewport. Asi la primera frame
    // muestra el fog/exposure/tonemap guardados, sin un flash a defaults.
    if (m_scene && m_sceneRenderer) {
        m_sceneRenderer->applyEnvironmentFromScene(*m_scene);
    }

    m_project = std::move(loaded);
    m_currentMapPath = m_project->defaultMap;
    m_projectDirty = false;
    addToRecentProjects(std::filesystem::absolute(moodproj));
    updateWindowTitle();
    m_ui.setStatusMessage("Proyecto abierto: " + m_project->name);
    Log::editor()->info("Proyecto abierto: {}", m_project->name);
    return true;
}

void EditorApplication::addToRecentProjects(const std::filesystem::path& moodproj) {
    const auto canonical = std::filesystem::absolute(moodproj);
    m_recentProjects.erase(
        std::remove_if(m_recentProjects.begin(), m_recentProjects.end(),
            [&canonical](const std::filesystem::path& p) {
                return std::filesystem::absolute(p) == canonical;
            }),
        m_recentProjects.end());
    m_recentProjects.insert(m_recentProjects.begin(), canonical);
    if (m_recentProjects.size() > 8) m_recentProjects.resize(8);
    m_ui.setRecentProjects(m_recentProjects);
}

void EditorApplication::handleSave() {
    // Modelo Unity/Godot: el editor siempre tiene un proyecto activo (el
    // modal Welcome bloquea la entrada al editor sin proyecto). Ctrl+S
    // siempre guarda sobre el proyecto actual.
    if (!m_project.has_value()) {
        Log::editor()->warn("Guardar: no hay proyecto activo (estado invalido)");
        return;
    }

    const auto mapPath = m_project->root / m_currentMapPath;
    std::filesystem::create_directories(mapPath.parent_path());
    try {
        SceneSerializer::save(m_map, m_currentMapPath.stem().generic_string(),
                              m_scene.get(), *m_assetManager, mapPath);
        ProjectSerializer::save(*m_project);
        m_projectDirty = false;
        updateWindowTitle();
        m_ui.setStatusMessage("Guardado: " + m_project->name);
        Log::editor()->info("Proyecto guardado ({}): {}",
            m_project->name, mapPath.generic_string());
    } catch (const std::exception& e) {
        Log::editor()->warn("Guardar fallo: {}", e.what());
        pfd::message("MoodEngine", std::string("Error al guardar: ") + e.what(),
                     pfd::choice::ok, pfd::icon::error);
    }
}

void EditorApplication::handleSaveAs() {
    // "Guardar como" completo queda diferido: requiere UI multi-mapa por
    // proyecto (el schema .moodproj lo soporta pero el editor todavia
    // opera sobre el defaultMap). Ver pendientes del Hito 6.
    if (m_project.has_value()) {
        pfd::message("MoodEngine — Guardar como",
                     "Guardar como todavia no esta implementado.\n"
                     "Usa Ctrl+S para guardar sobre el proyecto actual,\n"
                     "o Archivo > Cerrar proyecto y crear uno nuevo.",
                     pfd::choice::ok, pfd::icon::info);
    }
}

void EditorApplication::handleCloseProject() {
    if (!m_project.has_value()) return;
    if (!confirmDiscardChanges()) return;

    Log::editor()->info("Cerrando proyecto: {}", m_project->name);
    m_project.reset();
    m_currentMapPath.clear();
    m_projectDirty = false;
    // El mapa de prueba queda como "fondo" mientras el modal Welcome esta
    // abierto (el usuario lo ve borroso detras del popup). Ayuda visualmente
    // a distinguir "editor sin proyecto" vs "proyecto vacio".
    buildInitialTestMap();
    rebuildSceneFromMap();
    updateWindowTitle();
}

void EditorApplication::handlePackageProject() {
    if (!m_project.has_value()) return;

    // Si hay cambios sin guardar, ofrecer guardar primero — el packager
    // copia el .moodproj de disco, no el estado en memoria.
    if (m_projectDirty) {
        const auto choice = pfd::message(
            "MoodEngine — Empaquetar proyecto",
            "Tenes cambios sin guardar. Empaquetar el proyecto guardado actualmente "
            "no incluira esos cambios. Guardar primero?",
            pfd::choice::yes_no_cancel,
            pfd::icon::warning).result();
        if (choice == pfd::button::cancel) return;
        if (choice == pfd::button::yes) {
            handleSave();
            if (m_projectDirty) {
                Log::editor()->warn("Empaquetado abortado: el guardado fallo");
                return;
            }
        }
    }

    auto selection = pfd::select_folder(
        "Carpeta destino del paquete",
        m_project->root.parent_path().generic_string()).result();
    if (selection.empty()) return;

    // Hito 41 fix-up: si el destino `<selection>/<projectName>` ya existe,
    // pedir confirmacion al user antes de sobreescribir.
    const std::filesystem::path outDir =
        std::filesystem::path(selection) / m_project->name;
    if (std::filesystem::exists(outDir)) {
        const auto choice = pfd::message(
            "MoodEngine — Carpeta existe",
            "La carpeta:\n" + outDir.generic_string() + "\n\n"
            "ya existe. Sobreescribir su contenido?\n\n"
            "(SI = continuar y reemplazar archivos del paquete; "
            "NO = cancelar empaquetado)",
            pfd::choice::yes_no,
            pfd::icon::warning).result();
        if (choice != pfd::button::yes) {
            Log::editor()->info(
                "Empaquetado cancelado: el destino '{}' ya existe",
                outDir.generic_string());
            return;
        }
    }

    // Localizar el .exe del editor — junto a el viven MoodPlayer.exe,
    // SDL2*.dll, assets/, shaders/.
    char* base = SDL_GetBasePath();
    const std::filesystem::path exeDir = base
        ? std::filesystem::path(base)
        : std::filesystem::current_path();
    if (base) SDL_free(base);

#ifdef NDEBUG
    constexpr bool kIsDebug = false;
#else
    constexpr bool kIsDebug = true;
#endif

    PackageBuilder::BuildConfig cfg{
        *m_project,
        m_project->root / (m_project->name + ".moodproj"),
        std::filesystem::path(selection),
        exeDir,
        kIsDebug
    };
    const auto result = PackageBuilder::build(cfg);

    if (result.ok) {
        const std::string msg =
            "Paquete creado en:\n" + result.outputDir.generic_string() +
            "\n\n" + std::to_string(result.filesCopied) + " archivos copiados.";
        pfd::message("MoodEngine — Empaquetado completo",
                      msg, pfd::choice::ok, pfd::icon::info);
        Log::editor()->info("Empaquetado OK: {} ({} archivos)",
                             result.outputDir.generic_string(), result.filesCopied);
    } else {
        pfd::message("MoodEngine — Empaquetado fallo",
                      "Error: " + result.error,
                      pfd::choice::ok, pfd::icon::error);
        Log::editor()->error("Empaquetado fallo: {}", result.error);
    }
}

void EditorApplication::handleNewScript() {
    namespace fs = std::filesystem;
    const fs::path scriptsDir = fs::path("assets") / "scripts";
    std::error_code ec;
    fs::create_directories(scriptsDir, ec);

    auto selection = pfd::save_file(
        "Nuevo Script Lua",
        (scriptsDir / "nuevo.lua").generic_string(),
        {"Lua scripts", "*.lua"}).result();
    if (selection.empty()) return;

    fs::path target = fs::path(selection);
    if (target.extension() != ".lua") target += ".lua";

    // Forzar dentro de assets/scripts/ — si el usuario eligio otro path
    // (algun escritorio random), movemos el target a assets/scripts/
    // con el nombre que eligio. Sin esto el AssetManager (que mira
    // bajo `assets/` para resolver paths logicos como
    // "scripts/foo.lua") no lo encontraria.
    const fs::path scriptsAbs = fs::absolute(scriptsDir, ec);
    fs::path targetAbs = fs::absolute(target, ec);
    if (targetAbs.parent_path() != scriptsAbs) {
        const fs::path moved = scriptsDir / target.filename();
        Log::editor()->info(
            "Nuevo Script: '{}' fuera de assets/scripts/, redirigido a '{}'",
            target.generic_string(), moved.generic_string());
        target = moved;
    }

    if (fs::exists(target)) {
        const auto choice = pfd::message(
            "MoodEngine — Nuevo Script",
            "El archivo '" + target.generic_string() + "' ya existe. Sobreescribir?",
            pfd::choice::yes_no, pfd::icon::warning).result();
        if (choice != pfd::button::yes) return;
    }

    std::ofstream f(target);
    if (!f.is_open()) {
        Log::editor()->error("Nuevo Script: no se pudo crear '{}'",
                              target.generic_string());
        return;
    }
    // Template minimo: la firma que ScriptSystem espera. El comentario
    // del cuerpo da una pista de los bindings disponibles sin abrumar.
    f << "-- Auto-generado por MoodEngine.\n"
      << "-- Bindings disponibles: self.tag, self.transform.position/.rotationEuler/.scale,\n"
      << "-- log.info(msg), log.warn(msg), hud.setHp(v)/setAmmo(v)/setPaused(b).\n"
      << "\n"
      << "function onUpdate(self, dt)\n"
      << "    -- self.transform.position.x = self.transform.position.x + dt\n"
      << "end\n";
    f.close();

    Log::editor()->info("Nuevo Script creado: '{}'", target.generic_string());
    // Refrescar el Asset Browser para que el script nuevo aparezca en
    // la seccion Scripts sin reabrir el editor.
    m_ui.assetBrowser().rescan();
}

void EditorApplication::loadEditorState() {
    const auto path = std::filesystem::path(".mood") / "editor_state.json";
    std::ifstream in(path);
    if (!in.is_open()) return;

    nlohmann::json j;
    try { in >> j; } catch (...) { return; }

    // Preferencias globales.
    m_debugDraw = j.value("debugDraw", false);

    // Lista de proyectos recientes. El mas reciente esta al principio.
    // Se carga aca para alimentar el modal Welcome — pero NO se auto-abre
    // ninguno (el dev pidio explicitamente que cada relanzamiento arranque
    // limpio con la ventana de bienvenida; ver memoria de feedback). Si en
    // el futuro se quiere "auto-abrir el ultimo" como toggle de preferencias,
    // poner el `tryOpenProjectPath(m_recentProjects.front())` detras de un
    // flag opt-in.
    m_recentProjects.clear();
    if (j.contains("recentProjects") && j["recentProjects"].is_array()) {
        for (const auto& p : j["recentProjects"]) {
            m_recentProjects.emplace_back(p.get<std::string>());
        }
    }
    m_ui.setRecentProjects(m_recentProjects);
}

void EditorApplication::saveEditorState() const {
    std::error_code ec;
    std::filesystem::create_directories(".mood", ec);
    if (ec) return;

    nlohmann::json j;
    j["debugDraw"] = m_debugDraw;
    j["recentProjects"] = nlohmann::json::array();
    for (const auto& p : m_recentProjects) {
        j["recentProjects"].push_back(p.generic_string());
    }

    std::ofstream out(std::filesystem::path(".mood") / "editor_state.json");
    if (!out.is_open()) return;
    out << j.dump(2);
}

} // namespace Mood
