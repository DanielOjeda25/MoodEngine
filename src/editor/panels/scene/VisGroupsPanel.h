#pragma once

// F2H33: panel "Grupos" del editor de mapas. Lista los VisGroups del Scene
// con toggle hide/show, count de miembros, menu contextual (Renombrar /
// Color / Eliminar / Asignar seleccion). Boton "+ Nuevo grupo" arriba.
//
// Inyectado por EditorApplication: Scene + EditorUI (para leer
// SelectionSet) + HistoryStack (para empujar comandos undoable).
//
// Visible solo en workspace "Editor de mapas" (oculto por default;
// applyDefaultVisibilityForWorkspace lo activa).

#include "core/Types.h"
#include "editor/panels/IPanel.h"

#include <string>

namespace Mood {

class Scene;
class EditorUI;
class HistoryStack;

class VisGroupsPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Grupos"; }

    void setScene(Scene* s) { m_scene = s; }
    void setEditorUi(EditorUI* ui) { m_ui = ui; }
    void setHistoryStack(HistoryStack* h) { m_history = h; }

private:
    Scene* m_scene = nullptr;
    EditorUI* m_ui = nullptr;
    HistoryStack* m_history = nullptr;

    /// Buffer del rename inline (solo el grupo siendo renombrado a la vez).
    u64 m_renamingGroupId = 0;
    char m_renameBuffer[64] = {};
};

} // namespace Mood
