// F2H24: Inspector — AnimatorComponent (clip / speed / playing / loop).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "engine/animation/clips/AnimationClip.h"  // F2H49: metadata externa
#include "engine/assets/manager/AssetManager.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/render/resources/MeshAsset.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Mood {

namespace {

/// @brief F2H49: deriva un alias por defecto del path logico del clip.
///        `characters/player/anim_walk.fbx` → `walk`. Si el filename no
///        sigue la convencion `anim_*`, devuelve el stem completo
///        (`my_dance.fbx` → `my_dance`). Si nada de eso aplica, `clip`.
std::string defaultAliasFromPath(const std::string& logicalPath) {
    const auto slash = logicalPath.find_last_of('/');
    std::string stem = (slash == std::string::npos)
                        ? logicalPath
                        : logicalPath.substr(slash + 1);
    const auto dot = stem.find_last_of('.');
    if (dot != std::string::npos) stem = stem.substr(0, dot);
    constexpr std::string_view k_prefix = "anim_";
    if (stem.rfind(k_prefix, 0) == 0) stem = stem.substr(k_prefix.size());
    return stem.empty() ? std::string{"clip"} : stem;
}

} // namespace

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

    // ----------------------------------------------------------------------
    // F2H49: External clips standalone (FBX anim-only adjuntos por alias).
    // Cuando `anim.clipName` matchea un alias de esta lista, el AnimationSystem
    // prioriza el clip standalone sobre los embebidos del MeshAsset y resuelve
    // el bind contra el skeleton via `bindClipToSkeleton` (cacheado en
    // `externalBindCache`). Asi un mismo `anim_idle.fbx` se reusa entre player
    // y npc sin duplicar tracks.
    // ----------------------------------------------------------------------
    if (m_assets != nullptr) {
        ImGui::TextUnformatted(
            I18n::T("editor.panel.inspector.animator.external_clips").c_str());

        constexpr usize k_aliasBufSize = 64;
        std::optional<usize> pendingRemoveIdx;

        for (usize i = 0; i < anim.externalClips.size(); ++i) {
            auto& [alias, clipId] = anim.externalClips[i];
            ImGui::PushID(static_cast<int>(i));

            // Play (set clipName al alias).
            if (ImGui::SmallButton(ICON_FA_PLAY)) {
                anim.clipName = alias;
                anim.time = 0.0f;
                m_editedThisFrame = true;
            }
            ImGui::SameLine();

            // Alias editable. Stack buffer chico — un alias largo no aporta.
            char buf[k_aliasBufSize] = {0};
            const usize copyN = std::min(alias.size(), k_aliasBufSize - 1);
            std::memcpy(buf, alias.data(), copyN);
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputText("##alias", buf, k_aliasBufSize)) {
                alias.assign(buf);
                m_editedThisFrame = true;
            }
            ImGui::SameLine();

            // Path + metadata (clip id resuelto).
            const AnimationClip* clip = m_assets->getAnimationClip(clipId);
            const std::string path = m_assets->animationClipPathOf(clipId);
            if (clip != nullptr && !clip->tracks.empty()) {
                ImGui::TextDisabled("%s  [%u tracks, %.2fs]",
                                    path.c_str(),
                                    static_cast<u32>(clip->tracks.size()),
                                    clip->duration);
            } else {
                // Clip vacio: el id quedo apuntando al slot 0 (missing).
                // Mostramos el path igual para que el usuario lo identifique.
                ImGui::TextDisabled("%s  (missing)", path.c_str());
            }

            ImGui::SameLine();
            if (ImGui::SmallButton(ICON_FA_MINUS "##remove")) {
                pendingRemoveIdx = i;
            }

            ImGui::PopID();
        }

        if (pendingRemoveIdx.has_value()) {
            const usize idx = *pendingRemoveIdx;
            const AnimationClipAssetId clipId = anim.externalClips[idx].second;
            anim.externalClips.erase(anim.externalClips.begin() + idx);
            anim.externalBindCache.erase(clipId);
            m_editedThisFrame = true;
        }

        // Drop target: boton vacio que acepta `MOOD_ANIMCLIP_ASSET` del
        // AssetBrowser. Alias por defecto = filename stem sin prefijo
        // `anim_`. Si colisiona con uno existente, NO se agrega — el user
        // tiene que renombrar uno primero.
        ImGui::Button(
            I18n::T("editor.panel.inspector.animator.drop_clip_hint").c_str(),
            ImVec2(-1.0f, 28.0f));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload =
                    ImGui::AcceptDragDropPayload("MOOD_ANIMCLIP_ASSET")) {
                const auto clipId = *static_cast<const AnimationClipAssetId*>(
                    payload->Data);
                const std::string defaultAlias = defaultAliasFromPath(
                    m_assets->animationClipPathOf(clipId));
                const bool aliasCollides = std::any_of(
                    anim.externalClips.begin(), anim.externalClips.end(),
                    [&](const auto& p) { return p.first == defaultAlias; });
                if (!aliasCollides) {
                    anim.externalClips.emplace_back(defaultAlias, clipId);
                    m_editedThisFrame = true;
                } else {
                    ImGui::SetTooltip("%s",
                        I18n::T("editor.panel.inspector.animator.alias_in_use")
                            .c_str());
                }
            }
            ImGui::EndDragDropTarget();
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
