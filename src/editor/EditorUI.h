#pragma once

// Coordinador de toda la UI ImGui del editor. Agrega el dockspace, la menu
// bar, los paneles acoplables y la status bar. No hace init/shutdown de
// ImGui ni gestiona input: eso vive en EditorApplication.

#include "editor/Dockspace.h"
#include "editor/EditorMode.h"
#include "editor/MenuBar.h"
#include "editor/StatusBar.h"
#include "editor/panels/AssetBrowserPanel.h"
#include "editor/panels/ConsolePanel.h"
#include "editor/panels/HierarchyPanel.h"
#include "editor/panels/InspectorPanel.h"
#include "editor/panels/ViewportPanel.h"
#include "engine/scene/Entity.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Mood {

class EditorUI {
public:
    EditorUI();

    /// @brief Dibuja un frame completo de UI. Llamar entre ImGui::NewFrame()
    ///        y ImGui::Render().
    void draw(bool& requestQuit);

    /// @brief Actualiza el FPS mostrado en la status bar.
    void setFps(float fps) { m_statusBar.setFps(fps); }

    /// @brief Mensaje libre de la status bar (ej. "Guardado" tras un save).
    ///        Persiste hasta que se cambie de nuevo.
    void setStatusMessage(std::string msg) { m_statusBar.setMessage(std::move(msg)); }

    /// @brief Lista de paneles, expuesta para que el menu "Ver" pueda togglear
    ///        su visibilidad.
    const std::vector<IPanel*>& panels() const { return m_panels; }

    /// @brief Acceso al panel Viewport para inyectar el framebuffer desde
    ///        EditorApplication y leer el tamano deseado.
    ViewportPanel& viewport() { return m_viewport; }

    /// @brief Acceso al dockspace. MenuBar lo usa para pedir "Restablecer
    ///        layout" cuando el usuario lo elige desde el menu Ver.
    Dockspace& dockspace() { return m_dockspace; }

    /// @brief Acceso al panel Asset Browser para inyectarle el AssetManager
    ///        desde EditorApplication y leer la seleccion actual.
    AssetBrowserPanel& assetBrowser() { return m_assetBrowser; }

    /// @brief Acceso al panel Hierarchy para inyectarle el Scene desde
    ///        EditorApplication.
    HierarchyPanel& hierarchy() { return m_hierarchy; }

    /// @brief Acceso al panel Inspector (se le inyecta el Scene para
    ///        conocer la entidad seleccionada y editar sus componentes).
    InspectorPanel& inspector() { return m_inspector; }

    /// @brief Entidad seleccionada por el panel Hierarchy (compartida con
    ///        el Inspector). Entity default-constructed = sin seleccion.
    Entity selectedEntity() const { return m_selectedEntity; }
    void setSelectedEntity(Entity e) { m_selectedEntity = e; }

    /// @brief Demo del Hito 8: request para crear una entidad con
    ///        ScriptComponent apuntando a `scripts/rotator.lua`.
    ///        `EditorApplication` la consume despues de `draw()`.
    void requestSpawnRotator() { m_spawnRotatorRequested = true; }
    bool consumeSpawnRotatorRequest() {
        const bool r = m_spawnRotatorRequested;
        m_spawnRotatorRequested = false;
        return r;
    }

    /// @brief Demo del Hito 9: request para crear una entidad con
    ///        AudioSourceComponent (beep.wav loop + 3D) en una esquina del
    ///        mapa. `EditorApplication` la consume despues de `draw()`.
    void requestSpawnAudioSource() { m_spawnAudioSourceRequested = true; }
    bool consumeSpawnAudioSourceRequest() {
        const bool r = m_spawnAudioSourceRequested;
        m_spawnAudioSourceRequested = false;
        return r;
    }

    /// @brief Demo del Hito 11: request para crear una entidad con
    ///        LightComponent (Point, blanco, intensidad 1, radius 12)
    ///        suspendida sobre el centro del mapa.
    void requestSpawnPointLight() { m_spawnPointLightRequested = true; }
    bool consumeSpawnPointLightRequest() {
        const bool r = m_spawnPointLightRequested;
        m_spawnPointLightRequested = false;
        return r;
    }

    /// @brief Demo del Hito 12: request para spawnear una caja fisica
    ///        (Dynamic, Box, 1 m x 1 m x 1 m, 5 kg) a 6 m de altura. Cae
    ///        por gravedad hasta apoyarse en el suelo / tiles solidos.
    void requestSpawnPhysicsBox() { m_spawnPhysicsBoxRequested = true; }
    bool consumeSpawnPhysicsBoxRequest() {
        const bool r = m_spawnPhysicsBoxRequested;
        m_spawnPhysicsBoxRequested = false;
        return r;
    }

    /// @brief Modo actual del editor. EditorApplication es quien lo cambia;
    ///        la UI lo consulta para mostrarlo en la status bar y el label
    ///        del boton Play/Stop en la menu bar.
    EditorMode mode() const { return m_mode; }
    void setMode(EditorMode m) { m_mode = m; }

    /// @brief Solicitud de toggle del modo emitida por la UI (boton Play).
    ///        EditorApplication la consume con `consumeTogglePlayRequest`
    ///        luego de `draw()`.
    void requestTogglePlay() { m_togglePlayRequested = true; }
    bool consumeTogglePlayRequest() {
        const bool r = m_togglePlayRequested;
        m_togglePlayRequested = false;
        return r;
    }

    /// @brief Accion de proyecto solicitada desde el menu (Nuevo, Abrir,
    ///        Guardar, etc.). Solo puede haber una por frame.
    void requestProjectAction(ProjectAction a) { m_projectAction = a; }
    ProjectAction consumeProjectAction() {
        const auto a = m_projectAction;
        m_projectAction = ProjectAction::None;
        return a;
    }

    /// @brief Estado actual del proyecto (lo setea EditorApplication). La UI
    ///        lo usa para grayar items del menu cuando no hay proyecto
    ///        abierto y para decidir si mostrar el modal Welcome.
    bool hasProject() const { return m_hasProject; }
    void setHasProject(bool v) { m_hasProject = v; }

    /// @brief Lista de proyectos recientes (mostrada en el modal Welcome).
    ///        `path.stem()` se usa como label del boton.
    void setRecentProjects(std::vector<std::filesystem::path> paths) {
        m_recentProjects = std::move(paths);
    }

    /// @brief Si el modal Welcome solicito abrir un proyecto especifico
    ///        (click en un reciente), el path queda aca. `EditorApplication`
    ///        lo consume despues de `draw()`.
    std::optional<std::filesystem::path> consumeOpenProjectPath() {
        auto p = std::move(m_openProjectPath);
        m_openProjectPath.reset();
        return p;
    }

private:
    Dockspace m_dockspace;
    MenuBar m_menuBar;
    StatusBar m_statusBar;

    ViewportPanel m_viewport;
    HierarchyPanel m_hierarchy;
    InspectorPanel m_inspector;
    AssetBrowserPanel m_assetBrowser;
    ConsolePanel m_console;
    Entity m_selectedEntity;

    std::vector<IPanel*> m_panels;

    EditorMode m_mode = EditorMode::Editor;
    bool m_togglePlayRequested = false;

    ProjectAction m_projectAction = ProjectAction::None;
    bool m_hasProject = false;
    bool m_spawnRotatorRequested = false;
    bool m_spawnAudioSourceRequested = false;
    bool m_spawnPointLightRequested = false;
    bool m_spawnPhysicsBoxRequested = false;
    std::vector<std::filesystem::path> m_recentProjects;
    std::optional<std::filesystem::path> m_openProjectPath;

    /// @brief Dibuja un modal bloqueante con [Nuevo Proyecto] [Abrir Proyecto]
    ///        + lista de recientes. Se ejecuta SOLO cuando `m_hasProject` es
    ///        false. Bloquea toda interaccion con el editor hasta que el
    ///        usuario elija.
    void drawWelcomeModal();
};

} // namespace Mood
