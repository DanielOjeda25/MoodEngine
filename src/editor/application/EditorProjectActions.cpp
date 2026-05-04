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

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/packaging/PackageBuilder.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/components/Components.h"

#include <SDL.h>
#include <imgui.h>  // F2H7: SaveIniSettingsToMemory para persistencia de workspace

#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/ProjectSerializer.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/world/grid/GridMap.h"  // F2H8: handleNewMap
#include "engine/world/csg/Brush.h"     // F2H11: handleAddBoxBrush
#include "engine/scene/components/BrushComponent.h"

#include <algorithm>  // F2H8: std::remove_if en handleDeleteCurrentMap
#include <cstdio>     // F2H11: snprintf para naming unico

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
    m_ui.requestWorkspaceSwitch(0);  // siempre arranca en el primer workspace

    syncMapsSnapshot();  // F2H8: poblar el menu "Archivo > Mapa".

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

void EditorApplication::syncMapsSnapshot() {
    if (!m_project.has_value()) {
        m_ui.setProjectMapsSnapshot({}, {}, {});
        return;
    }
    m_ui.setProjectMapsSnapshot(m_project->maps,
                                  m_currentMapPath,
                                  m_project->defaultMap);
}

// F2H8: gestion multi-mapa intra-proyecto.

namespace {
// Sanitiza un filename para Windows shell (mismo helper que
// processSavePrefabRequest del Hito 14).
std::string sanitizeMapName(std::string s) {
    for (char& c : s) {
        switch (c) {
            case '<': case '>': case ':': case '"':
            case '/': case '\\': case '|':
            case '?': case '*': c = '_'; break;
            default: break;
        }
    }
    return s;
}
} // namespace

void EditorApplication::handleSaveMapAs() {
    if (!m_project.has_value()) return;

    const auto mapsDir = m_project->root / "maps";
    std::error_code ec;
    std::filesystem::create_directories(mapsDir, ec);

    // Default filename: <currentName>_copy.moodmap.
    const std::string baseName = sanitizeMapName(
        m_currentMapPath.stem().generic_string());
    const std::string defaultName = baseName + "_copy.moodmap";

    const auto sel = pfd::save_file(
        "Guardar mapa como",
        (mapsDir / defaultName).string(),
        {"Mapas MoodEngine (*.moodmap)", "*.moodmap"},
        pfd::opt::none).result();
    if (sel.empty()) {
        Log::editor()->info("Guardar mapa como: cancelado");
        return;
    }

    auto outPath = std::filesystem::path(sel);
    if (outPath.extension() != ".moodmap") {
        outPath += ".moodmap";
    }

    // Path relativo al project root para persistir en project.maps.
    std::error_code rec;
    const auto relPath = std::filesystem::relative(
        std::filesystem::absolute(outPath, rec),
        std::filesystem::absolute(m_project->root, rec), rec);
    const auto storedPath = (rec || relPath.empty())
        ? outPath
        : relPath;

    try {
        SceneSerializer::save(m_map, outPath.stem().generic_string(),
                                m_scene.get(), *m_assetManager, outPath);
    } catch (const std::exception& e) {
        Log::editor()->warn("Guardar mapa como fallo: {}", e.what());
        pfd::message("MoodEngine", std::string("Error: ") + e.what(),
                       pfd::choice::ok, pfd::icon::error);
        return;
    }

    // Agregar al project.maps si no existe; switch al nuevo.
    bool alreadyInList = false;
    for (const auto& m : m_project->maps) {
        if (m.generic_string() == storedPath.generic_string()) {
            alreadyInList = true;
            break;
        }
    }
    if (!alreadyInList) m_project->maps.push_back(storedPath);
    m_currentMapPath = storedPath;
    m_projectDirty = true;
    updateWindowTitle();
    syncMapsSnapshot();
    m_ui.setStatusMessage("Mapa guardado como: " + outPath.filename().generic_string());
    Log::editor()->info("Mapa guardado como: {}", outPath.generic_string());
}

void EditorApplication::handleNewMap() {
    if (!m_project.has_value()) return;
    if (!confirmDiscardChanges()) return;

    const auto mapsDir = m_project->root / "maps";
    std::error_code ec;
    std::filesystem::create_directories(mapsDir, ec);

    const auto sel = pfd::save_file(
        "Nuevo mapa",
        (mapsDir / "untitled.moodmap").string(),
        {"Mapas MoodEngine (*.moodmap)", "*.moodmap"},
        pfd::opt::none).result();
    if (sel.empty()) return;

    auto outPath = std::filesystem::path(sel);
    if (outPath.extension() != ".moodmap") outPath += ".moodmap";

    // Mapa vacio default (16x16, tileSize 3.0 — mismo que buildInitialTestMap).
    GridMap fresh(16u, 16u, 3.0f);
    Scene freshScene;

    try {
        SceneSerializer::save(fresh, outPath.stem().generic_string(),
                                &freshScene, *m_assetManager, outPath);
    } catch (const std::exception& e) {
        Log::editor()->warn("Nuevo mapa fallo: {}", e.what());
        pfd::message("MoodEngine", std::string("Error: ") + e.what(),
                       pfd::choice::ok, pfd::icon::error);
        return;
    }

    std::error_code rec;
    const auto relPath = std::filesystem::relative(
        std::filesystem::absolute(outPath, rec),
        std::filesystem::absolute(m_project->root, rec), rec);
    const auto storedPath = (rec || relPath.empty()) ? outPath : relPath;

    // Agregar y switch.
    bool exists = false;
    for (const auto& m : m_project->maps) {
        if (m.generic_string() == storedPath.generic_string()) {
            exists = true; break;
        }
    }
    if (!exists) m_project->maps.push_back(storedPath);

    handleOpenMap(storedPath);
    m_projectDirty = true;
    updateWindowTitle();
    syncMapsSnapshot();
    m_ui.setStatusMessage("Mapa nuevo: " + outPath.filename().generic_string());
}

void EditorApplication::handleOpenMap(const std::filesystem::path& mapPath) {
    if (!m_project.has_value()) return;
    if (!confirmDiscardChanges()) return;

    const auto absMapPath = mapPath.is_absolute()
        ? mapPath
        : (m_project->root / mapPath);
    auto saved = SceneSerializer::load(absMapPath, *m_assetManager);
    if (!saved.has_value()) {
        pfd::message("MoodEngine",
                       "No se pudo cargar el mapa: " + absMapPath.generic_string(),
                       pfd::choice::ok, pfd::icon::error);
        return;
    }

    m_map = std::move(saved->map);
    rebuildSceneFromMap();
    if (m_scene) {
        SceneLoader::applyEntitiesToScene(*saved, *m_scene, *m_assetManager);
    }
    if (m_scene && m_sceneRenderer) {
        m_sceneRenderer->applyEnvironmentFromScene(*m_scene);
    }

    m_currentMapPath = mapPath;
    m_projectDirty = false;
    updateWindowTitle();
    syncMapsSnapshot();
    m_ui.setStatusMessage("Mapa abierto: " + mapPath.filename().generic_string());
    Log::editor()->info("Mapa abierto: {}", mapPath.generic_string());
}

void EditorApplication::handleSetCurrentMapAsDefault() {
    if (!m_project.has_value()) return;
    if (m_project->defaultMap.generic_string() == m_currentMapPath.generic_string()) {
        m_ui.setStatusMessage("Este mapa ya es el default");
        return;
    }
    m_project->defaultMap = m_currentMapPath;
    m_projectDirty = true;
    syncMapsSnapshot();
    m_ui.setStatusMessage("Default = " + m_currentMapPath.filename().generic_string());
    Log::editor()->info("Default map: {}", m_currentMapPath.generic_string());
}

void EditorApplication::handleDeleteCurrentMap() {
    if (!m_project.has_value()) return;
    if (m_project->maps.size() <= 1u) {
        pfd::message("MoodEngine — Eliminar mapa",
                       "No se puede eliminar el ultimo mapa del proyecto.\n"
                       "Crea otro primero (Archivo > Mapa > Nuevo mapa).",
                       pfd::choice::ok, pfd::icon::warning);
        return;
    }

    const auto choice = pfd::message(
        "MoodEngine — Eliminar mapa",
        "Eliminar '" + m_currentMapPath.filename().generic_string() +
            "'? Esta accion no se puede deshacer.",
        pfd::choice::yes_no, pfd::icon::warning).result();
    if (choice != pfd::button::yes) return;

    const auto absPath = m_project->root / m_currentMapPath;
    std::error_code ec;
    std::filesystem::remove(absPath, ec);
    if (ec) {
        Log::editor()->warn("Eliminar mapa: no se pudo borrar '{}': {}",
                              absPath.generic_string(), ec.message());
    }

    // Quitar de project.maps.
    const auto deleted = m_currentMapPath;
    auto it = std::remove_if(m_project->maps.begin(), m_project->maps.end(),
        [&](const std::filesystem::path& p) {
            return p.generic_string() == deleted.generic_string();
        });
    m_project->maps.erase(it, m_project->maps.end());

    // Si era el default, reasignar.
    if (m_project->defaultMap.generic_string() == deleted.generic_string()) {
        m_project->defaultMap = m_project->maps.front();
    }

    // Cargar el primer mapa restante.
    handleOpenMap(m_project->maps.front());
    m_projectDirty = true;
    updateWindowTitle();
    syncMapsSnapshot();
    m_ui.setStatusMessage("Mapa eliminado: " + deleted.filename().generic_string());
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

// ----------------------------------------------------------------------------
// F2H11: handleAddBoxBrush — crea una entidad con BrushComponent (Box 1x1x1).
// ----------------------------------------------------------------------------

void EditorApplication::handleAddBoxBrush() {
    if (!m_scene) return;

    // Tag unico (Brush_Box_01, Brush_Box_02, ...).
    int suffix = 1;
    std::string tagName;
    while (true) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "Brush_Box_%02d", suffix);
        tagName = buf;
        bool collision = false;
        m_scene->forEach<TagComponent>(
            [&](Entity, TagComponent& tag) {
                if (tag.name == tagName) collision = true;
            });
        if (!collision) break;
        ++suffix;
    }

    Entity e = m_scene->createEntity(tagName);
    auto& t = e.getComponent<TransformComponent>();
    // Posicion arbitraria un poco arriba del suelo para que sea visible.
    t.position = glm::vec3(0.0f, 1.0f, 0.0f);
    t.scale    = glm::vec3(1.0f);

    BrushComponent bc;
    bc.brush = Csg::makeBoxBrush(glm::mat4(1.0f));
    bc.material = 0;     // material default (slot 0 del AssetManager)
    bc.dirty   = true;
    e.addComponent<BrushComponent>(std::move(bc));

    Log::editor()->info("Anadir Box Brush: '{}' en (0, 1, 0)", tagName);
    pushCreatedEntities({e}, "Anadir Box Brush");
}

} // namespace Mood
