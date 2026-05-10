#include "editor/panels/scene/InspectorPanel.h"

#include "editor/selection/SelectionSet.h"           // F2H13
#include "editor/ui/EditorUI.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/scene/components/BrushComponent.h"  // F2H11
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <functional>
#include <vector>

namespace Mood {

namespace {

// F2H44 Bloque A: case-insensitive substring match para el search del
// popup Add Component.
bool fuzzyMatch(const std::string& haystack, const char* needle) {
    if (needle == nullptr || needle[0] == '\0') return true;
    std::string a = haystack;
    std::string b = needle;
    std::transform(a.begin(), a.end(), a.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    std::transform(b.begin(), b.end(), b.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return a.find(b) != std::string::npos;
}

} // namespace

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
        ImGui::TextDisabled("%s", I18n::T("editor.panel.inspector.ui_not_injected").c_str());
        ImGui::End();
        return;
    }

    Entity e = m_ui->selectedEntity();
    if (!e) {
        ImGui::TextDisabled("%s", I18n::T("editor.panel.inspector.no_selection").c_str());
        ImGui::TextDisabled("%s", I18n::T("editor.panel.inspector.no_selection_hint").c_str());
        ImGui::End();
        return;
    }

    // F2H13: header "+N adicionales" cuando hay multi-seleccion.
    // Inspector solo edita la `active`; multi-edit es diferido (excepto
    // Transform — ver renderTransformSection).
    {
        const SelectionSet& set = m_ui->selectionSet();
        if (set.selected.size() > 1) {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.multi_extra",
                        static_cast<int>(set.selected.size() - 1)).c_str());
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

    // F2H44 Bloque A: boton "+ Add Component" + popup. Al final del
    // dispatch para no interrumpir el flow de lectura de los componentes
    // existentes.
    renderAddComponentSection(e);

    ImGui::End();
}

void InspectorPanel::renderAddComponentSection(Entity e) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Centrado horizontal: calculamos width del boton + padding y
    // empujamos el cursor para que quede al medio.
    const std::string label = I18n::T("editor.panel.inspector.add.button");
    const ImVec2 textSz = ImGui::CalcTextSize(label.c_str());
    const float btnW = textSz.x + ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
    const float avail = ImGui::GetContentRegionAvail().x;
    if (avail > btnW) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - btnW) * 0.5f);
    }
    if (ImGui::Button(label.c_str(), ImVec2(btnW, 0.0f))) {
        m_addComponentSearch[0] = '\0';  // reset search cada vez
        ImGui::OpenPopup("##add_component_popup");
    }

    drawAddComponentPopup(e);
}

void InspectorPanel::drawAddComponentPopup(Entity e) {
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (!ImGui::BeginPopup("##add_component_popup", flags)) return;

    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.add.popup_title").c_str());
    ImGui::Separator();

    // Search input. Auto-focus al primer frame del popup para typing
    // inmediato sin click extra.
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    ImGui::SetNextItemWidth(280.0f);
    ImGui::InputTextWithHint("##add_search",
                              I18n::T("editor.panel.inspector.add.search").c_str(),
                              m_addComponentSearch,
                              sizeof(m_addComponentSearch));
    ImGui::Separator();

    // Spec de cada componente agregable: nombre + descripcion + grupo +
    // hash flag (`hasComponent<X>(e)`) + addFunctor (closure que llama
    // `e.addComponent<X>(default)`).
    struct Item {
        const char* nameKey;
        const char* descKey;
        const char* catKey;
        bool        alreadyHas;
        std::function<void()> addFn;
    };
    std::vector<Item> items;
    items.reserve(12);

    auto add = [&](const char* nameKey, const char* descKey,
                    const char* catKey, bool alreadyHas,
                    std::function<void()> fn) {
        items.push_back({nameKey, descKey, catKey, alreadyHas, std::move(fn)});
    };

    // Render
    add("component.name.mesh_renderer", "component.desc.mesh_renderer",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<MeshRendererComponent>(),
        [e]() mutable { e.addComponent<MeshRendererComponent>(); });
    add("component.name.light", "component.desc.light",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<LightComponent>(),
        [e]() mutable { e.addComponent<LightComponent>(); });
    add("component.name.environment", "component.desc.environment",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<EnvironmentComponent>(),
        [e]() mutable { e.addComponent<EnvironmentComponent>(); });
    add("component.name.animator", "component.desc.animator",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<AnimatorComponent>(),
        [e]() mutable { e.addComponent<AnimatorComponent>(); });
    add("component.name.particle_emitter", "component.desc.particle_emitter",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<ParticleEmitterComponent>(),
        [e]() mutable { e.addComponent<ParticleEmitterComponent>(); });
    add("component.name.camera", "component.desc.camera",
        "editor.panel.inspector.add.cat.render",
        e.hasComponent<CameraComponent>(),
        [e]() mutable { e.addComponent<CameraComponent>(); });

    // Physics
    add("component.name.rigid_body", "component.desc.rigid_body",
        "editor.panel.inspector.add.cat.physics",
        e.hasComponent<RigidBodyComponent>(),
        [e]() mutable { e.addComponent<RigidBodyComponent>(); });
    add("component.name.trigger", "component.desc.trigger",
        "editor.panel.inspector.add.cat.physics",
        e.hasComponent<TriggerComponent>(),
        [e]() mutable { e.addComponent<TriggerComponent>(); });

    // Audio
    add("component.name.audio_source", "component.desc.audio_source",
        "editor.panel.inspector.add.cat.audio",
        e.hasComponent<AudioSourceComponent>(),
        [e]() mutable { e.addComponent<AudioSourceComponent>(); });

    // Logic
    add("component.name.script", "component.desc.script",
        "editor.panel.inspector.add.cat.logic",
        e.hasComponent<ScriptComponent>(),
        [e]() mutable { e.addComponent<ScriptComponent>(); });
    add("component.name.nav_agent", "component.desc.nav_agent",
        "editor.panel.inspector.add.cat.logic",
        e.hasComponent<NavAgentComponent>(),
        [e]() mutable { e.addComponent<NavAgentComponent>(); });

    // World
    add("component.name.brush", "component.desc.brush",
        "editor.panel.inspector.add.cat.world",
        e.hasComponent<BrushComponent>(),
        [e]() mutable { e.addComponent<BrushComponent>(); });

    // Filtro: solo los que la entidad NO tiene + matchean el search.
    // Agrupados por categoria, en el orden original del registro.
    const char* lastCat = nullptr;
    bool any = false;
    for (auto& it : items) {
        if (it.alreadyHas) continue;
        const std::string nameTr = I18n::T(it.nameKey);
        if (!fuzzyMatch(nameTr, m_addComponentSearch)) continue;
        any = true;
        // Header del grupo si cambio de categoria.
        if (lastCat == nullptr ||
            std::strcmp(lastCat, it.catKey) != 0) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s",
                I18n::T(it.catKey).c_str());
            lastCat = it.catKey;
        }
        // Selectable con descripcion abajo.
        if (ImGui::Selectable(nameTr.c_str(), false,
                                ImGuiSelectableFlags_None,
                                ImVec2(360.0f, 0.0f))) {
            it.addFn();
            m_editedThisFrame = true;
            ImGui::CloseCurrentPopup();
        }
        // Descripcion en gris debajo del nombre, indentada.
        ImGui::Indent(16.0f);
        ImGui::TextDisabled("%s", I18n::T(it.descKey).c_str());
        ImGui::Unindent(16.0f);
    }

    if (!any) {
        ImGui::Spacing();
        if (m_addComponentSearch[0] != '\0') {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.add.no_match",
                        std::string(m_addComponentSearch)).c_str());
        } else {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.add.empty").c_str());
        }
    }

    ImGui::EndPopup();
}

} // namespace Mood
