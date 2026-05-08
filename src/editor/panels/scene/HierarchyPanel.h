#pragma once

// Panel que lista las entidades de la escena activa como un arbol plano
// (sin jerarquia padre-hijo todavia; eso entra cuando agreguemos
// ChildrenComponent o el equivalente). Click en un item actualiza la
// seleccion compartida con el Inspector via `EditorUI::setSelectedEntity`.
//
// F2H5: el render usa `ImGuiListClipper` para virtualizar — con escenas
// grandes (8K+ entidades) solo se procesan las entries visibles del
// scroll area, no todas. La lista se cachea en `m_entries` cada frame
// y se reusa el storage entre frames para no reallocar.

#include "editor/panels/IPanel.h"

#include <entt/entt.hpp>

#include <vector>

namespace Mood {

class Scene;
class EditorUI;
struct TagComponent;

/// @brief Una fila del Hierarchy: entidad + puntero al tag para evitar
///        un `getComponent` dentro del clipper (el get es barato pero
///        se llama por entry visible, no acumula).
struct HierarchyEntry {
    entt::entity handle{entt::null};
    const TagComponent* tag{nullptr};
};

/// @brief Recolecta las entidades con TagComponent en `out`. PURO: no
///        toca ImGui. `out` se limpia con `clear()` antes de rellenar
///        — el caller puede pasar el mismo vector entre frames para
///        reusar la capacidad.
void collectHierarchyEntries(Scene& scene, std::vector<HierarchyEntry>& out);

class HierarchyPanel : public IPanel {
public:
    void onImGuiRender() override;
    // F2H23: nombre visual "Escena" (mas descriptivo + consistencia
    // castellano). La clase sigue llamandose HierarchyPanel por compat
    // con la jerga interna del codigo + iniLayout existente del dev.
    const char* name() const override { return "Escena"; }

    /// @brief Inyectado por `EditorApplication` en el ctor. Non-owning.
    void setScene(Scene* scene) { m_scene = scene; }

    /// @brief La seleccion se guarda en `EditorUI` para que el Inspector
    ///        la lea sin acoplamiento panel-panel. Referencia no-owning.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    Scene* m_scene = nullptr;
    EditorUI* m_ui = nullptr;
    /// Cache de la lista de entries. Se rellena al inicio de cada
    /// `onImGuiRender` y se reusa el storage entre frames.
    std::vector<HierarchyEntry> m_entries;
};

} // namespace Mood
