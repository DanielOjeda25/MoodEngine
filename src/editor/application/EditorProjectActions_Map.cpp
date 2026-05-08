// F2H24: gestion multi-mapa intra-proyecto (F2H8).
// - syncMapsSnapshot (mantener menu "Archivo > Mapa" sincronizado)
// - handleSaveMapAs / handleNewMap / handleOpenMap
// - handleSetCurrentMapAsDefault / handleDeleteCurrentMap

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/render/scene_renderer/SceneRenderer.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/scene/core/Scene.h"
#include "engine/world/grid/GridMap.h"

#include <portable-file-dialogs.h>

#include <algorithm>
#include <string>

namespace Mood {

void EditorApplication::syncMapsSnapshot() {
    if (!m_project.has_value()) {
        m_ui.setProjectMapsSnapshot({}, {}, {});
        return;
    }
    m_ui.setProjectMapsSnapshot(m_project->maps,
                                  m_currentMapPath,
                                  m_project->defaultMap);
}

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
        // F2H26: persistir mesh compilada junto con los brushes.
        auto compiledMesh = buildSavedCompiledMeshFromScene(*m_scene, *m_assetManager);
        SceneSerializer::save(m_map, outPath.stem().generic_string(),
                                m_scene.get(), *m_assetManager, outPath,
                                &compiledMesh);
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

} // namespace Mood
