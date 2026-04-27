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
#include "engine/scene/Components.h"
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
    // Entidades no-tile persistidas (Hito 10 Bloque 6): se aplican DESPUES
    // del rebuildSceneFromMap que crea las Tile_X_Y. Si no hay entidades
    // (archivo v1 o mapa sin meshes dropeados), el loop no hace nada.
    if (m_scene) {
        for (const auto& se : savedMap->entities) {
            Entity e = m_scene->createEntity(se.tag);
            auto& t = e.getComponent<TransformComponent>();
            t.position      = se.position;
            t.rotationEuler = se.rotationEuler;
            t.scale         = se.scale;
            if (se.meshRenderer.has_value()) {
                const auto& mr = *se.meshRenderer;
                const MeshAssetId meshId = m_assetManager->loadMesh(mr.meshPath);
                // Hito 17 upgrader: el campo `materials` puede contener
                // paths de `.material` (v7) o paths de texturas (v6 con
                // back-compat). Detectamos por extension: si termina en
                // `.material` cargamos como material; si no, lo tratamos
                // como textura y la envolvemos en un material wrapper.
                std::vector<MaterialAssetId> mats;
                mats.reserve(mr.materials.size());
                for (const auto& path : mr.materials) {
                    if (path.empty()) {
                        mats.push_back(m_assetManager->missingMaterialId());
                        continue;
                    }
                    const bool isMaterial =
                        path.size() >= 9 &&
                        path.compare(path.size() - 9, 9, ".material") == 0;
                    if (isMaterial) {
                        mats.push_back(m_assetManager->loadMaterial(path));
                    } else {
                        const TextureAssetId tex =
                            m_assetManager->loadTexture(path);
                        mats.push_back(
                            m_assetManager->loadMaterialFromTexture(tex));
                    }
                }
                e.addComponent<MeshRendererComponent>(meshId, std::move(mats));
            }
            // Hito 11: aplicar LightComponent persistido si existe.
            if (se.light.has_value()) {
                const auto& sl = *se.light;
                LightComponent lc{};
                lc.type = (sl.type == "directional")
                    ? LightComponent::Type::Directional
                    : LightComponent::Type::Point;
                lc.color     = sl.color;
                lc.intensity = sl.intensity;
                lc.radius    = sl.radius;
                lc.direction = sl.direction;
                lc.enabled   = sl.enabled;
                lc.castShadows = sl.castShadows; // Hito 16
                e.addComponent<LightComponent>(lc);
            }
            // Hito 12: aplicar RigidBodyComponent persistido. El body se
            // materializa en el proximo frame via updateRigidBodies().
            if (se.rigidBody.has_value()) {
                const auto& srb = *se.rigidBody;
                RigidBodyComponent rb{};
                if (srb.type == "static")         rb.type = RigidBodyComponent::Type::Static;
                else if (srb.type == "kinematic") rb.type = RigidBodyComponent::Type::Kinematic;
                else                              rb.type = RigidBodyComponent::Type::Dynamic;
                if (srb.shape == "sphere")        rb.shape = RigidBodyComponent::Shape::Sphere;
                else if (srb.shape == "capsule")  rb.shape = RigidBodyComponent::Shape::Capsule;
                else                              rb.shape = RigidBodyComponent::Shape::Box;
                rb.halfExtents = srb.halfExtents;
                rb.mass = srb.mass;
                e.addComponent<RigidBodyComponent>(rb);
            }
            // Hito 15: aplicar EnvironmentComponent persistido si existe.
            if (se.environment.has_value()) {
                const auto& s = *se.environment;
                EnvironmentComponent env{};
                env.skyboxPath     = s.skyboxPath;
                if      (s.fogMode == "linear") env.fogMode = 1;
                else if (s.fogMode == "exp")    env.fogMode = 2;
                else if (s.fogMode == "exp2")   env.fogMode = 3;
                else                            env.fogMode = 0;
                env.fogColor       = s.fogColor;
                env.fogDensity     = s.fogDensity;
                env.fogLinearStart = s.fogLinearStart;
                env.fogLinearEnd   = s.fogLinearEnd;
                env.exposure       = s.exposure;
                if      (s.tonemapMode == "reinhard") env.tonemapMode = 1;
                else if (s.tonemapMode == "aces")     env.tonemapMode = 2;
                else                                  env.tonemapMode = 0;
                env.iblIntensity   = s.iblIntensity; // Hito 18
                e.addComponent<EnvironmentComponent>(env);
            }
            // Hito 14: link al prefab de origen, si la entidad fue
            // persistida con `prefab_path`. Sin propagacion bidireccional
            // por ahora — solo el breadcrumb.
            if (!se.prefabPath.empty()) {
                e.addComponent<PrefabLinkComponent>(se.prefabPath);
            }
        }
    }
    // Hito 15: aplicar el Environment del proyecto recien cargado YA, en
    // lugar de esperar al primer renderSceneToViewport. Asi la primera frame
    // muestra el fog/exposure/tonemap guardados, sin un flash a defaults.
    applyEnvironmentFromScene();

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
