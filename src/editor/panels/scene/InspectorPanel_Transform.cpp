// F2H24: Inspector — TransformComponent (multi-edit F2H23).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/Log.h"
#include "editor/selection/SelectionSet.h"
#include "editor/ui/EditorUI.h"
#include "engine/scene/components/BrushComponent.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <algorithm>

namespace Mood {

// TransformComponent
// Scale + rotation solo tienen efecto visible si hay MeshRenderer
// (o es un tile rendereado). Para Light/Audio puros (sin mesh)
// ocultarlos evita confundir al usuario con controles sin efecto.
void InspectorPanel::renderTransformSection(Entity e) {
    auto& t = e.getComponent<TransformComponent>();
    // F2H23: SeparatorText agrupa visualmente cada componente (antes
    // era TextDisabled + Separator separados; menos claro).
    ImGui::SeparatorText(ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Transform");

    // F2H23 fix: sliders Transform mas grandes + soportan multi-edit
    // sobre `selectionSet.selected`. Antes eran chicos y dificiles
    // de seleccionar; ademas solo modificaban la `active` aunque
    // hubiese N entidades seleccionadas.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 6.0f));

    // Helper: aplica un delta a TODAS las entidades del selectionSet
    // EXCEPTO la `active` (que ya fue modificada por el DragFloat3
    // directamente). Recibe `getter`/`setter` pq cada campo
    // (position/rotation/scale) tiene su propio acceso al
    // TransformComponent.
    auto applyDeltaToSelection = [&](const glm::vec3& delta,
                                        auto getter, auto setter,
                                        const char* opName) {
        if (delta.x == 0.0f && delta.y == 0.0f && delta.z == 0.0f) return;
        const SelectionSet& set = m_ui->selectionSet();
        if (set.selected.size() <= 1u) return;  // nada extra para editar
        u32 affected = 0;
        for (const Entity& other : set.selected) {
            if (other.handle() == e.handle()) continue;  // skip active
            Entity oCopy = other;
            if (!oCopy.hasComponent<TransformComponent>()) continue;
            auto& ot = oCopy.getComponent<TransformComponent>();
            setter(ot, getter(ot) + delta);
            ++affected;
        }
        if (affected > 0u) {
            Log::editor()->info(
                "[inspector multi-edit] {} delta=({:.3f},{:.3f},{:.3f}) "
                "aplicado a {} entidad(es) extra (active fue editada por "
                "el slider directamente)",
                opName, delta.x, delta.y, delta.z, affected);
        }
    };

    // --- position ---
    const glm::vec3 prePos = t.position;
    if (ImGui::DragFloat3("position##tr", &t.position.x, 0.05f)) {
        m_editedThisFrame = true;
        applyDeltaToSelection(t.position - prePos,
            [](TransformComponent& tc) { return tc.position; },
            [](TransformComponent& tc, const glm::vec3& v) { tc.position = v; },
            "position");
    }
    detail::helpMarker("Posicion en metros (X derecha, Y arriba, Z atras).\n"
                "Multi-seleccion: el mismo delta se aplica a todas.");
    detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, t.position,
        [](Entity& en, const glm::vec3& v) {
            en.getComponent<TransformComponent>().position = v;
        },
        "Mover entidad (Inspector)");

    // Rotation + scale: visibles si la entidad tiene geometria
    // visible (MeshRenderer / BrushComponent) o TriggerComponent
    // (rotation afecta el OBB del trigger — Hito 40 B). Para
    // Light/Audio sin geometria siguen ocultos.
    const bool showRotScale =
        e.hasComponent<MeshRendererComponent>() ||
        e.hasComponent<BrushComponent>() ||  // F2H14
        e.hasComponent<TriggerComponent>();
    if (showRotScale) {
        // --- rotation ---
        const glm::vec3 preRot = t.rotationEuler;
        if (ImGui::DragFloat3("rotation (deg)##tr", &t.rotationEuler.x, 0.5f)) {
            m_editedThisFrame = true;
            applyDeltaToSelection(t.rotationEuler - preRot,
                [](TransformComponent& tc) { return tc.rotationEuler; },
                [](TransformComponent& tc, const glm::vec3& v) { tc.rotationEuler = v; },
                "rotation");
        }
        detail::helpMarker("Rotacion en grados Euler (X, Y, Z) — orden YXZ.\n"
                    "Multi-seleccion: el mismo delta se aplica a todas.");
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, t.rotationEuler,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<TransformComponent>().rotationEuler = v;
            },
            "Rotar entidad (Inspector)");

        // --- scale ---
        const glm::vec3 preScale = t.scale;
        if (ImGui::DragFloat3("scale##tr", &t.scale.x, 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
            applyDeltaToSelection(t.scale - preScale,
                [](TransformComponent& tc) { return tc.scale; },
                [](TransformComponent& tc, const glm::vec3& v) {
                    // clamp al mismo rango que el DragFloat3.
                    tc.scale = glm::vec3(
                        std::clamp(v.x, 0.01f, 100.0f),
                        std::clamp(v.y, 0.01f, 100.0f),
                        std::clamp(v.z, 0.01f, 100.0f));
                },
                "scale");
        }
        detail::helpMarker("Escala (rango 0.01-100). 1.0 = tamano original.\n"
                    "Multi-seleccion: el mismo delta se aplica a todas.");
        detail::pushEditIfDone<glm::vec3>(m_editTracker, m_ui, e, t.scale,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<TransformComponent>().scale = v;
            },
            "Escalar entidad (Inspector)");
    }

    ImGui::PopStyleVar();  // FramePadding

    // F2H23: hint de multi-seleccion. Solo aparece cuando hay >1.
    const SelectionSet& set = m_ui->selectionSet();
    if (set.selected.size() > 1u) {
        ImGui::TextDisabled(
            "(multi-edit: %zu entidades — delta aplicado a todas)",
            set.selected.size());
    }
}

} // namespace Mood
