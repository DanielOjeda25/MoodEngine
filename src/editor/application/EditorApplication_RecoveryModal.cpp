// F3H25 — Modal de recuperación de sesión.
//
// Aparece tras `tryOpenProjectPath` cuando el lock file del proyecto
// estaba huérfano (PID muerto) Y el autosave en `.autosave/` es más
// reciente que el `.moodmap` canónico. El dev decide: cargar el
// autosave (mostrando los cambios no guardados de la sesión perdida) o
// descartarlo (volver al estado canónico).
//
// El método se llama por frame desde `pumpUiRequests`; sólo abre el
// popup en el frame donde `m_recoveryModalPending=true` y lo deja
// abierto hasta que el dev clickea uno de los botones. La acción
// "Restaurar" recarga el mapa desde el autosave y lo deja dirty (el
// .moodmap canónico todavía no tiene esa data).

#include "editor/application/EditorApplication.h"

#include "core/Log.h"
#include "core/Toasts.h"
#include "core/i18n/I18n.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/scene/core/Scene.h"
#include "engine/scene/serialization/SceneLoader.h"
#include "engine/scene/serialization/SceneSerializer.h"
#include "engine/render/scene_renderer/SceneRenderer.h"

#include <imgui.h>

namespace Mood {

void EditorApplication::processRecoveryModal() {
    if (!m_recoveryModalPending) return;
    if (!m_project.has_value()) {
        // Estado raro: el proyecto se cerró antes de que el dev decidiera.
        // Resetear el flag sin tocar disco.
        m_recoveryModalPending = false;
        m_recoveryAutosavePath.clear();
        return;
    }

    constexpr const char* kModalId = "recovery_modal";
    ImGui::OpenPopup(kModalId);
    const auto& vp = *ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp.GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(kModalId, nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(I18n::T("editor.recovery_modal.title").c_str());
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", I18n::T("editor.recovery_modal.body").c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("%s",
            m_recoveryAutosavePath.filename().generic_string().c_str());
        ImGui::Spacing();
        ImGui::Separator();

        const float buttonW = 130.0f;
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float windowW = ImGui::GetContentRegionAvail().x;
        const float buttonsW = buttonW * 2.0f + spacing;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (windowW - buttonsW) * 0.5f);

        if (ImGui::Button(I18n::T("editor.recovery_modal.restore").c_str(),
                          ImVec2(buttonW, 0))) {
            // Cargar el autosave como si fuera el mapa actual. Marcar
            // dirty: el .moodmap canónico no tiene esa data hasta que el
            // dev haga save manual.
            auto saved = SceneSerializer::load(m_recoveryAutosavePath, *m_assetManager);
            if (saved.has_value()) {
                m_map = std::move(saved->map);
                rebuildSceneFromMap();
                if (m_scene) {
                    SceneLoader::applyEntitiesToScene(*saved, *m_scene, *m_assetManager);
                }
                ensureEnvironmentExists();
                if (m_scene && m_sceneRenderer) {
                    m_sceneRenderer->applyEnvironmentFromScene(*m_scene);
                }
                m_projectDirty = true;
                updateWindowTitle();
                Log::editor()->info("[recovery] sesión restaurada desde '{}'",
                                    m_recoveryAutosavePath.generic_string());
                Toasts::pushSuccess(I18n::T("editor.toast.session_recovered"));
                // No borrar el autosave: el dev podría querer guardarlo
                // manualmente y el próximo Save lo limpiará igual.
            } else {
                Log::editor()->warn("[recovery] no se pudo cargar '{}'",
                                    m_recoveryAutosavePath.generic_string());
                Toasts::pushError(I18n::T("editor.toast.session_recovery_failed"));
            }
            m_recoveryModalPending = false;
            m_recoveryAutosavePath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(I18n::T("editor.recovery_modal.discard").c_str(),
                          ImVec2(buttonW, 0))) {
            // Descartar: borrar autosave. El estado actual (cargado del
            // .moodmap canónico en tryOpenProjectPath) queda como activo.
            m_autosave.clearOnDisk();
            Log::editor()->info("[recovery] autosave descartado por el dev");
            m_recoveryModalPending = false;
            m_recoveryAutosavePath.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

} // namespace Mood
