// F2H24: Inspector — AnimatorComponent (clip / speed / playing / loop).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/i18n/I18n.h"  // F2H43
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <memory>
#include <string>
#include <vector>

namespace Mood {

// AnimatorComponent (Hito 19)
// Combo de clips poblado desde el MeshAsset de la entidad. Editar
// el clipName resetea `time` para que el clip nuevo arranque desde
// el frame 0.
void InspectorPanel::renderAnimatorSection(Entity e) {
    auto& anim = e.getComponent<AnimatorComponent>();
    ImGui::SeparatorText(ICON_FA_PERSON_RUNNING " Animator");

    // Necesitamos el MeshAsset para listar los clips. Lo resolvemos
    // via MeshRenderer (mismo flujo que el AnimationSystem).
    if (m_assets != nullptr && e.hasComponent<MeshRendererComponent>()) {
        const auto& mr = e.getComponent<MeshRendererComponent>();
        const MeshAsset* mesh = m_assets->getMesh(mr.mesh);
        if (mesh != nullptr && !mesh->animations.empty()) {
            std::vector<std::string> clipNames;
            clipNames.reserve(mesh->animations.size());
            for (const auto& c : mesh->animations) clipNames.push_back(c.name);

            int currentIdx = 0;
            for (int i = 0; i < static_cast<int>(clipNames.size()); ++i) {
                if (clipNames[i] == anim.clipName) { currentIdx = i; break; }
            }
            const std::string clipLabel = I18n::T("editor.panel.inspector.animator.clip") + "##anim";
            if (ImGui::BeginCombo(clipLabel.c_str(),
                                    clipNames[currentIdx].c_str())) {
                for (int i = 0; i < static_cast<int>(clipNames.size()); ++i) {
                    const bool selected = (currentIdx == i);
                    if (ImGui::Selectable(clipNames[i].c_str(), selected)) {
                        // Hito 40 F: undo del clipName combo (string).
                        const std::string oldClip = anim.clipName;
                        const std::string newClip = clipNames[i];
                        if (oldClip != newClip) {
                            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
                            if (h != nullptr) {
                                auto cmd = std::make_unique<EditPropertyCommand<std::string>>(
                                    e, oldClip, newClip,
                                    [](Entity& en, const std::string& v) {
                                        auto& a = en.getComponent<AnimatorComponent>();
                                        a.clipName = v;
                                        a.time = 0.0f;  // reset al cambiar
                                    },
                                    "Cambiar Animator clip");
                                h->push(std::move(cmd));
                            } else {
                                anim.clipName = newClip;
                                anim.time = 0.0f;
                            }
                            m_editedThisFrame = true;
                        }
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::TextDisabled(
                "%s",
                I18n::T("editor.panel.inspector.animator.duration_time",
                        mesh->animations[currentIdx].duration, anim.time).c_str());
        } else {
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.animator.no_clips").c_str());
        }
    }

    const std::string speedLabel = I18n::T("editor.panel.inspector.animator.speed") + "##anim";
    if (ImGui::DragFloat(speedLabel.c_str(), &anim.speed, 0.05f, 0.0f, 10.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, anim.speed,
        [](Entity& en, const f32& v) {
            en.getComponent<AnimatorComponent>().speed = v;
        },
        "Editar animator speed");
    const std::string playingLabel = I18n::T("editor.panel.inspector.animator.playing") + "##anim";
    if (ImGui::Checkbox(playingLabel.c_str(), &anim.playing)) { m_editedThisFrame = true; }
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, anim.playing,
        [](Entity& en, const bool& v) {
            en.getComponent<AnimatorComponent>().playing = v;
        },
        "Toggle animator playing");
    ImGui::SameLine();
    const std::string loopLabel = I18n::T("editor.panel.inspector.animator.loop") + "##anim";
    if (ImGui::Checkbox(loopLabel.c_str(), &anim.loop)) { m_editedThisFrame = true; }
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, anim.loop,
        [](Entity& en, const bool& v) {
            en.getComponent<AnimatorComponent>().loop = v;
        },
        "Toggle animator loop");
    ImGui::SameLine();
    const std::string resetLabel = I18n::T("editor.panel.inspector.animator.reset") + "##anim";
    if (ImGui::Button(resetLabel.c_str())) {
        anim.time = 0.0f;
        m_editedThisFrame = true;
    }
    ImGui::Separator();
}

} // namespace Mood
