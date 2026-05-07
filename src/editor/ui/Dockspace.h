#pragma once

// Configura una ventana ImGui "host" ocupando toda la viewport principal que
// aloja el DockSpace central del editor. El menu bar se dibuja dentro de esta
// ventana host para que quede integrado con el area acoplable.
//
// F2H7: el dockspace conoce el "kind" de workspace activo
// (Layout/Scripting/Profile/Materials) para construir layouts predefinidos
// distintos. Si el workspace tiene un iniLayout serializado, ImGui lo
// aplica via LoadIniSettingsFromMemory (en EditorUI::applyPendingWorkspaceSwitch);
// si no, el Dockspace construye el default del kind correspondiente.

#include <string>

namespace Mood {

class Dockspace {
public:
    /// @brief Comienza la ventana host. Debe llamarse antes de dibujar el
    ///        menu bar y los paneles. Devuelve true si la ventana quedo
    ///        abierta (siempre true actualmente, pero se devuelve por
    ///        simetria con el patron ImGui::Begin).
    bool begin();

    /// @brief Cierra la ventana host abierta con begin().
    void end();

    /// @brief Fuerza que el proximo `begin()` reconstruya el layout por
    ///        defecto ignorando lo persistido en `imgui.ini`. Util para el
    ///        item "Ver > Restablecer layout" cuando se agregan paneles
    ///        nuevos o el dev reorganizo y quiere volver a empezar.
    void requestResetToDefault() { m_resetRequested = true; }

    /// @brief F2H7: setea el nombre del workspace activo. El proximo
    ///        `begin()` que necesite reconstruir el layout (porque cambio
    ///        de workspace o se pidio reset) usara ese nombre para elegir
    ///        cual de los 4 layouts predefinidos aplicar. Default "Layout".
    void setActiveWorkspaceName(std::string name) {
        m_activeWorkspaceName = std::move(name);
    }

    /// @brief F2H7: indica al dockspace que debe RECONSTRUIR el layout en
    ///        el proximo begin() porque el workspace cambio. Distinto de
    ///        `requestResetToDefault` (que es trigger manual del user).
    ///        Lo llama `EditorUI::applyPendingWorkspaceSwitch` cuando el
    ///        nuevo workspace tiene `iniLayout` vacio.
    void requestRebuildForCurrentWorkspace() { m_rebuildRequested = true; }

private:
    bool m_buildAttempted = false;
    bool m_resetRequested = false;
    bool m_rebuildRequested = false;
    std::string m_activeWorkspaceName{"Modelar"};  // F2H22: default nuevo
};

} // namespace Mood
