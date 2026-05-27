// F3H19: AssetBrowserPanel modal de "Renombrar asset con cascada".
// Abre via right-click → "Renombrar..." en cualquier tab. Muestra
// preview de refs encontradas + InputText con el nuevo nombre. Al
// confirmar, deja en `m_pendingRename` un PendingRename que
// EditorApplication consume en `pumpUiRequests` para construir el
// RenameAssetCommand y push al history.
//
// Validación inline:
//   - El nuevo nombre no puede estar vacío.
//   - El nuevo nombre debe ser distinto al actual.
//   - El path destino NO puede existir en disco (decisión D2 — abort
//     vs sufijo automático).

#include "editor/panels/assets/AssetBrowserPanel.h"

#include "core/Log.h"
#include "core/i18n/I18n.h"
#include "engine/assets/refs/AssetRefIndex.h"
#include "engine/scene/core/Scene.h"

#include <imgui.h>

#include <filesystem>
#include <string>

namespace Mood {

namespace {

// Hace forward slashes y quita el prefijo "assets/" si lo trae.
std::string normalizeLogical(const std::string& p) {
    return asset_refs::normalizePath(p);
}

}  // namespace

void AssetBrowserPanel::openRenameModal(const std::string& logicalPath) {
    if (m_assetManager == nullptr || m_scene == nullptr) return;
    if (logicalPath.empty()) return;

    m_renameOldLogical = normalizeLogical(logicalPath);
    m_renameOldDisk = m_assetManager->resolvePath(m_renameOldLogical);

    // Default del nuevo nombre = solo el filename (sin carpetas), para que
    // el dev edite el stem sin tener que retocar el directorio.
    const std::filesystem::path p(m_renameOldLogical);
    m_renameNewName = p.filename().string();
    m_renameError.clear();

    // Snapshot de refs al abrir — si la scene cambia mientras el modal
    // está abierto, las refs del snapshot quedan stale (caso raro).
    m_renameRefsCache = asset_refs::findRefs(*m_scene, *m_assetManager,
                                              m_renameOldLogical);

    m_renameModalOpen = true;
}

void AssetBrowserPanel::drawRenameModal() {
    if (!m_renameModalOpen) return;

    const char* kModalId = "##rename_asset_modal";
    if (!ImGui::IsPopupOpen(kModalId)) {
        ImGui::OpenPopup(kModalId);
    }

    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(kModalId, &m_renameModalOpen,
                                  ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    ImGui::TextUnformatted(I18n::T("editor.asset_browser.rename_modal.title").c_str());
    ImGui::Separator();

    // Path actual (read-only para contexto).
    ImGui::TextDisabled("%s", m_renameOldLogical.c_str());
    ImGui::Spacing();

    // InputText con el nuevo nombre.
    ImGui::TextUnformatted(I18n::T("editor.asset_browser.rename_modal.new_name").c_str());
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%s", m_renameNewName.c_str());
    if (ImGui::InputText("##new_name", buf, sizeof(buf))) {
        m_renameNewName = buf;
        m_renameError.clear();
    }

    // Preview de refs.
    const usize refCount = m_renameRefsCache.size();
    ImGui::Spacing();
    if (refCount == 0) {
        ImGui::TextDisabled(
            "%s", I18n::T("editor.asset_browser.rename_modal.no_refs").c_str());
    } else {
        ImGui::Text(
            "%s: %zu",
            I18n::T("editor.asset_browser.rename_modal.refs_count").c_str(),
            refCount);
        // Lista compacta (max 8 visibles, scroll si hay más).
        ImGui::BeginChild("##refs_list",
                           ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 8.0f),
                           true);
        for (const auto& site : m_renameRefsCache) {
            if (site.entity) {
                ImGui::BulletText("Entity %u",
                    static_cast<unsigned int>(site.entity.handle()));
            } else if (!site.materialPath.empty()) {
                ImGui::BulletText("Material: %s", site.materialPath.c_str());
            } else {
                ImGui::BulletText("(ref)");
            }
        }
        ImGui::EndChild();
    }

    // Error inline si hay.
    if (!m_renameError.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f),
                            "%s", m_renameError.c_str());
    }

    ImGui::Spacing();
    ImGui::Separator();

    const bool canConfirm = !m_renameNewName.empty()
        && m_renameNewName != std::filesystem::path(m_renameOldLogical).filename().string();

    if (!canConfirm) ImGui::BeginDisabled();
    if (ImGui::Button(I18n::T("editor.asset_browser.rename_modal.confirm").c_str())) {
        // Validación final: construir el newLogical (preservar la carpeta del
        // oldLogical) y chequear conflict con disco.
        std::filesystem::path parent =
            std::filesystem::path(m_renameOldLogical).parent_path();
        std::string newLogical = parent.empty()
            ? m_renameNewName
            : (parent.generic_string() + "/" + m_renameNewName);
        const auto newDisk = m_assetManager->resolvePath(newLogical);

        if (newLogical == m_renameOldLogical) {
            m_renameError = I18n::T("editor.asset_browser.rename_modal.error_same");
        } else if (std::filesystem::exists(newDisk)) {
            m_renameError = I18n::T("editor.asset_browser.rename_modal.error_exists");
        } else {
            PendingRename pr;
            pr.oldDiskPath = m_renameOldDisk;
            pr.newDiskPath = newDisk;
            pr.oldLogical = m_renameOldLogical;
            pr.newLogical = std::move(newLogical);
            pr.refs = std::move(m_renameRefsCache);
            m_pendingRename = std::move(pr);
            m_renameModalOpen = false;
            ImGui::CloseCurrentPopup();
        }
    }
    if (!canConfirm) ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.asset_browser.rename_modal.cancel").c_str())) {
        m_renameModalOpen = false;
        m_renameRefsCache.clear();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void AssetBrowserPanel::addRenameContextMenu(const std::string& logicalPath) {
    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem(I18n::T("editor.asset_browser.rename").c_str())) {
            openRenameModal(logicalPath);
        }
        ImGui::EndPopup();
    }
}

}  // namespace Mood
