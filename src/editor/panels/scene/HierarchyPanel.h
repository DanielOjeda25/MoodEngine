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

#include <unordered_set>
#include <vector>

namespace Mood {

class Scene;
class EditorUI;
class AssetManager;  // F3H9: serializeComponent en click-derecho paste
struct TagComponent;

/// @brief Una fila del Hierarchy: entidad + puntero al tag para evitar
///        un `getComponent` dentro del clipper (el get es barato pero
///        se llama por entry visible, no acumula).
///        F3H27: añadidos `depth` (profundidad en la jerarquia parent/
///        child, 0 = root) + `hasChildren` (true si tiene al menos 1
///        hijo, para render del arrow expand/collapse).
struct HierarchyEntry {
    entt::entity handle{entt::null};
    const TagComponent* tag{nullptr};
    int depth{0};
    bool hasChildren{false};
};

/// @brief Recolecta las entidades con TagComponent en `out`. PURO: no
///        toca ImGui. `out` se limpia con `clear()` antes de rellenar
///        — el caller puede pasar el mismo vector entre frames para
///        reusar la capacidad.
///        F3H27: emite en DFS pre-order (padre antes que hijos), respeta
///        `collapsed` (handles cuyas subtrees se ocultan). depth=0 para
///        roots. Las entries derivadas (VehicleWheelMarker, Environment)
///        se siguen ocultando.
void collectHierarchyEntries(Scene& scene, std::vector<HierarchyEntry>& out,
                              const std::unordered_set<entt::entity>& collapsed = {});

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

    /// @brief F3H9: inyectado para el paste de componentes en click-
    ///        derecho (serializeComponent + applyPayload necesitan
    ///        resolver texture refs del ParticleEmitter).
    void setAssetManager(AssetManager* am) { m_assets = am; }

private:
    Scene* m_scene = nullptr;
    EditorUI* m_ui = nullptr;
    AssetManager* m_assets = nullptr;
    /// Cache de la lista de entries. Se rellena al inicio de cada
    /// `onImGuiRender` y se reusa el storage entre frames.
    std::vector<HierarchyEntry> m_entries;
    /// F3H27: handles cuya subtree esta colapsada (no se muestran sus
    /// hijos). Persistido en memoria mientras el panel viva — no se
    /// serializa.
    std::unordered_set<entt::entity> m_collapsed;
};

} // namespace Mood
