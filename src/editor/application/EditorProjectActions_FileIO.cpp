// F2H24: handlers del lifecycle de proyecto:
// - handleNewProject (pfd save dialog + ProjectSerializer::createNewProject)
// - handleOpenProject + tryOpenProjectPath (pfd open + ProjectSerializer::load)
// - handleSave / handleSaveAs (delega en handleSaveMapAs)
// - handleCloseProject
// - handleNewScript (template Lua)

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/serialization/ProjectSerializer.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"

#include <imgui.h>
#include <portable-file-dialogs.h>

#include <fstream>

namespace Mood {

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
    // F2H26: persistir tambien la mesh compilada para que el Player
    // la cargue directo en lugar de procesar brushes.
    auto compiledMesh = buildSavedCompiledMeshFromScene(*m_scene, *m_assetManager);
    SceneSerializer::save(m_map, created->name, m_scene.get(), *m_assetManager,
                          mapPath, &compiledMesh);

    m_project = std::move(created);
    m_currentMapPath = m_project->defaultMap;
    m_projectDirty = false;
    addToRecentProjects(m_project->root / (m_project->name + ".moodproj"));
    updateWindowTitle();
    syncMapsSnapshot();
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

    // F2H7: poblar el WorkspaceManager con los workspaces persistidos del
    // proyecto. Si el array esta vacio (proyecto recien creado o pre-F2H7),
    // setWorkspaces({}) cae a los 4 defaults.
    m_ui.workspaceManager().setWorkspaces(m_project->workspaces);

    // F2H35 Bloque E: restaurar el toggle "Nombres" persistido por
    // proyecto. Default true (post-F2H35) preservado si el .moodproj
    // pre-F2H35 no tiene el campo.
    m_ui.setShowEntityLabels(m_project->showEntityLabels);

    // F2H22 polish: al cargar un proyecto SIEMPRE arrancar en Modelar
    // (workspace 0) con layout limpio + visibility default. Antes el
    // editor recordaba el ultimo workspace + el ultimo layout (via
    // imgui.ini global) y dejaba ventanas flotantes / panels en
    // posiciones raras del estado previo. El dev pidio explicitamente
    // "que siempre se quede en Modelar layout, sin ventanas flotantes,
    // todo fijo en una posicion facil de comenzar a familiarizarse".
    //
    // No usamos requestWorkspaceSwitch porque chequea
    // `target == activeIndex` y returna early (setWorkspaces ya dejo
    // active en 0); aplicamos el flow manualmente:
    //   1) Setear active=0 (Modelar).
    //   2) Setear el nombre en el Dockspace para el dispatcher.
    //   3) Aplicar visibility default del workspace.
    //   4) Si workspace 0 tiene iniLayout custom guardado en .moodproj,
    //      cargarlo (pisa el imgui.ini global). Si esta vacio, cargar
    //      string "" para limpiar el state previo + pedir rebuild
    //      con DockBuilder.
    m_ui.workspaceManager().setActiveByIndex(0);
    const auto& ws0 = m_ui.workspaceManager().activeWorkspace();
    m_ui.dockspace().setActiveWorkspaceName(ws0.name);
    m_ui.applyDefaultVisibilityForWorkspace(ws0.name);
    if (ws0.iniLayout.empty()) {
        ImGui::LoadIniSettingsFromMemory("", 0);
        m_ui.dockspace().requestRebuildForCurrentWorkspace();
    } else {
        ImGui::LoadIniSettingsFromMemory(ws0.iniLayout.c_str(),
                                          ws0.iniLayout.size());
    }

    syncMapsSnapshot();  // F2H8: poblar el menu "Archivo > Mapa".

    m_ui.setStatusMessage("Proyecto abierto: " + m_project->name);
    Log::editor()->info("Proyecto abierto: {}", m_project->name);
    return true;
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
        // F2H26: persistir mesh compilada junto con los brushes.
        auto compiledMesh = buildSavedCompiledMeshFromScene(*m_scene, *m_assetManager);
        SceneSerializer::save(m_map, m_currentMapPath.stem().generic_string(),
                              m_scene.get(), *m_assetManager, mapPath,
                              &compiledMesh);

        // F2H7: capturar el ini ImGui actual al workspace activo + copiar
        // todos los workspaces al Project antes de serializar.
        {
            size_t size = 0;
            const char* iniData = ImGui::SaveIniSettingsToMemory(&size);
            if (iniData != nullptr && size > 0) {
                m_ui.workspaceManager().captureCurrentLayout(
                    std::string(iniData, size));
            }
            m_project->workspaces = m_ui.workspaceManager().workspaces();
        }
        // F2H35 Bloque E: capturar el toggle "Nombres" actual al
        // proyecto antes de serializar.
        m_project->showEntityLabels = m_ui.showEntityLabels();
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
    // F2H8: redirige al nuevo handler `handleSaveMapAs` — Save As del
    // editor = guardar el mapa actual con otro nombre dentro del
    // proyecto. "Save Project As" (copiar carpeta entera) sigue
    // out-of-scope (ver PLAN_HITO_F2H8.md).
    handleSaveMapAs();
}

void EditorApplication::handleCloseProject() {
    if (!m_project.has_value()) return;
    if (!confirmDiscardChanges()) return;

    Log::editor()->info("Cerrando proyecto: {}", m_project->name);
    m_project.reset();
    m_currentMapPath.clear();
    m_projectDirty = false;
    syncMapsSnapshot();  // F2H8: limpiar snapshot del menu Mapa.
    // El mapa de prueba queda como "fondo" mientras el modal Welcome esta
    // abierto (el usuario lo ve borroso detras del popup). Ayuda visualmente
    // a distinguir "editor sin proyecto" vs "proyecto vacio".
    buildInitialTestMap();
    rebuildSceneFromMap();
    updateWindowTitle();
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

} // namespace Mood
