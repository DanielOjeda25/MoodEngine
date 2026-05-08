#include "editor/panels/scene/InspectorPanel.h"

#include "editor/selection/SelectionSet.h"           // F2H13
#include "editor/ui/EditorUI.h"
#include "engine/scene/components/BrushComponent.h"  // F2H11
#include "engine/scene/components/Components.h"

#include <imgui.h>

namespace Mood {

// F2H24: nucleo del Inspector. El cuerpo de cada componente vive en
// archivos parciales `InspectorPanel_<Dominio>.cpp` (Transform,
// MeshRenderer, Light, Script, Physics, Audio, Animation, Particles,
// Brush, Misc). Este archivo solo:
//   1) chequea visibilidad / inyeccion de UI / entidad seleccionada
//   2) muestra el header de multi-seleccion
//   3) dispatchea a cada `renderXxxSection(e)` segun los componentes
//      presentes en `e`.
// Sin logica de edicion ni helpers; eso queda en los partials +
// InspectorPanel_Internal.h.
void InspectorPanel::onImGuiRender() {
    if (!visible) return;

    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (m_ui == nullptr) {
        ImGui::TextDisabled("Editor UI no inyectado");
        ImGui::End();
        return;
    }

    Entity e = m_ui->selectedEntity();
    if (!e) {
        ImGui::TextDisabled("No hay entidad seleccionada");
        ImGui::TextDisabled("Click en el panel Hierarchy para elegir una.");
        ImGui::End();
        return;
    }

    // F2H13: header "+N adicionales" cuando hay multi-seleccion.
    // Inspector solo edita la `active`; multi-edit es diferido (excepto
    // Transform — ver renderTransformSection).
    {
        const SelectionSet& set = m_ui->selectionSet();
        if (set.selected.size() > 1) {
            ImGui::TextDisabled(
                "+%d entidad(es) adicional(es) seleccionada(s) — solo se "
                "edita la activa",
                static_cast<int>(set.selected.size() - 1));
            ImGui::Separator();
        }
    }

    // Dispatch por componente. El orden es el mismo que tenia
    // `onImGuiRender` antes del split (F2H23+).
    if (e.hasComponent<TagComponent>())              renderTagSection(e);
    if (e.hasComponent<TransformComponent>())        renderTransformSection(e);
    if (e.hasComponent<MeshRendererComponent>())     renderMeshRendererSection(e);
    if (e.hasComponent<CameraComponent>())           renderCameraSection(e);
    if (e.hasComponent<LightComponent>())            renderLightSection(e);
    if (e.hasComponent<EnvironmentComponent>())      renderEnvironmentSection(e);
    if (e.hasComponent<ScriptComponent>())           renderScriptSection(e);
    if (e.hasComponent<RigidBodyComponent>())        renderRigidBodySection(e);
    if (e.hasComponent<AudioSourceComponent>())      renderAudioSourceSection(e);
    if (e.hasComponent<AnimatorComponent>())         renderAnimatorSection(e);
    if (e.hasComponent<ParticleEmitterComponent>())  renderParticleEmitterSection(e);
    if (e.hasComponent<TriggerComponent>())          renderTriggerSection(e);
    if (e.hasComponent<BrushComponent>())            renderBrushSection(e);

    ImGui::End();
}

} // namespace Mood
