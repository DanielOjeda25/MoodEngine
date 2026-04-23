#pragma once

// Panel que lista las entidades de la escena activa como un arbol plano
// (sin jerarquia padre-hijo todavia; eso entra cuando agreguemos
// ChildrenComponent o el equivalente). Click en un item actualiza la
// seleccion compartida con el Inspector via `EditorUI::setSelectedEntity`.

#include "editor/panels/IPanel.h"

namespace Mood {

class Scene;
class EditorUI;

class HierarchyPanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Hierarchy"; }

    /// @brief Inyectado por `EditorApplication` en el ctor. Non-owning.
    void setScene(Scene* scene) { m_scene = scene; }

    /// @brief La seleccion se guarda en `EditorUI` para que el Inspector
    ///        la lea sin acoplamiento panel-panel. Referencia no-owning.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    Scene* m_scene = nullptr;
    EditorUI* m_ui = nullptr;
};

} // namespace Mood
