// F2H24: Inspector — AudioSourceComponent (clip / volume / loop /
// playOnStart / is3D / preview).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/audio/clips/AudioClip.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace Mood {

// AudioSourceComponent (Hito 9)
// Dropdown poblado a partir de los clips cargados en el AssetManager.
// Boton "Reproducir" resetea `started` para que AudioSystem dispare de
// nuevo en el proximo update (util para preview sin entrar a Play Mode).
void InspectorPanel::renderAudioSourceSection(Entity e) {
    auto& asrc = e.getComponent<AudioSourceComponent>();
    ImGui::SeparatorText(ICON_FA_VOLUME_HIGH " AudioSource");

    // Lista de clips del manager como combo. Si aun no hay assets manager
    // inyectado, mostrar solo el id crudo.
    if (m_assets != nullptr) {
        std::vector<std::string> labels;
        labels.reserve(m_assets->audioCount());
        for (AudioAssetId i = 0; i < m_assets->audioCount(); ++i) {
            labels.push_back(m_assets->audioPathOf(i));
        }
        int current = static_cast<int>(asrc.clip);
        if (current < 0 || current >= static_cast<int>(labels.size())) {
            current = 0;
        }
        // BeginCombo para soportar paths largos con scroll.
        const std::string clipLabel = I18n::T("editor.panel.inspector.audio.clip") + "##as";
        if (ImGui::BeginCombo(clipLabel.c_str(), labels[current].c_str())) {
            for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
                const bool selected = (current == i);
                if (ImGui::Selectable(labels[i].c_str(), selected)) {
                    asrc.clip = static_cast<AudioAssetId>(i);
                    asrc.started = false; // obligar a re-arrancar con el nuevo clip
                    m_editedThisFrame = true;
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::Text("%s",
            I18n::T("editor.panel.inspector.audio.clip_id_no_assets",
                    asrc.clip).c_str());
    }

    const std::string volumeLabel = I18n::T("editor.panel.inspector.audio.volume") + "##as";
    if (ImGui::SliderFloat(volumeLabel.c_str(), &asrc.volume, 0.0f, 1.0f)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, asrc.volume,
        [](Entity& en, const f32& v) {
            en.getComponent<AudioSourceComponent>().volume = v;
        },
        "Editar audio volume");
    const std::string loopLabel = I18n::T("editor.panel.inspector.audio.loop") + "##as";
    if (ImGui::Checkbox(loopLabel.c_str(), &asrc.loop)) { m_editedThisFrame = true; }
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, asrc.loop,
        [](Entity& en, const bool& v) {
            en.getComponent<AudioSourceComponent>().loop = v;
        },
        "Toggle audio loop");
    ImGui::SameLine();
    const std::string playOnStartLabel = I18n::T("editor.panel.inspector.audio.play_on_start") + "##as";
    if (ImGui::Checkbox(playOnStartLabel.c_str(), &asrc.playOnStart)) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, asrc.playOnStart,
        [](Entity& en, const bool& v) {
            en.getComponent<AudioSourceComponent>().playOnStart = v;
        },
        "Toggle audio playOnStart");
    ImGui::SameLine();
    const std::string is3DLabel = I18n::T("editor.panel.inspector.audio.is_3d") + "##as";
    if (ImGui::Checkbox(is3DLabel.c_str(), &asrc.is3D)) { m_editedThisFrame = true; }
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, asrc.is3D,
        [](Entity& en, const bool& v) {
            en.getComponent<AudioSourceComponent>().is3D = v;
        },
        "Toggle audio is3D");

    // Preview: resetear `started` fuerza al AudioSystem a volver a
    // disparar la reproduccion en el proximo frame. Requiere playOnStart.
    const std::string playLabel = I18n::T("editor.panel.inspector.audio.play") + "##as";
    if (ImGui::Button(playLabel.c_str())) {
        asrc.started = false;
        if (!asrc.playOnStart) {
            // Preview implicito: encender playOnStart temporalmente no
            // es ideal (persiste). Mejor: advertir al usuario.
            ImGui::OpenPopup("audio_preview_note");
        }
    }
    if (ImGui::BeginPopup("audio_preview_note")) {
        ImGui::TextUnformatted(I18n::T("editor.panel.inspector.audio.preview_hint").c_str());
        ImGui::EndPopup();
    }
    ImGui::Separator();
}

} // namespace Mood
