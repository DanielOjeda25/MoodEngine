#include "editor/panels/project/AssetIssuesPanel.h"

#include "core/i18n/I18n.h"
#include "core/Log.h"
#include "editor/ui/IconsFontAwesome6.h"

#include <imgui.h>

namespace Mood {

namespace {

// Colores Inspector-style: rojo para BrokenRef, ambar para LoadFailed.
constexpr ImU32 kColorBroken     = IM_COL32(232,  92,  92, 255);  // rojo
constexpr ImU32 kColorLoadFailed = IM_COL32(232, 175,  60, 255);  // ambar

const char* iconForKind(asset_validation::IssueKind k) {
    switch (k) {
        case asset_validation::IssueKind::BrokenRef:  return ICON_FA_LINK_SLASH;
        case asset_validation::IssueKind::LoadFailed: return ICON_FA_TRIANGLE_EXCLAMATION;
    }
    return ICON_FA_CIRCLE_QUESTION;
}

ImU32 colorForKind(asset_validation::IssueKind k) {
    return k == asset_validation::IssueKind::BrokenRef
                ? kColorBroken : kColorLoadFailed;
}

} // namespace

void AssetIssuesPanel::refresh(Scene& scene, const AssetManager& assets) {
    m_issues = asset_validation::validateProject(scene, assets);
    m_refreshRequested = false;
    Log::editor()->info(
        "AssetIssuesPanel: refresh -> {} issues detectados", m_issues.size());
}

Entity AssetIssuesPanel::consumePendingSelect() {
    Entity e = m_pendingSelect;
    m_pendingSelect = Entity{};
    return e;
}

bool AssetIssuesPanel::isFieldBroken(Entity e, std::string_view path) const {
    if (!e || path.empty()) return false;
    for (const auto& is : m_issues) {
        if (is.entity == e && is.assetPath == path) return true;
    }
    return false;
}

void AssetIssuesPanel::onImGuiRender() {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(680.0f, 420.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(I18n::T("editor.panel.asset_issues.title").c_str(),
                       &visible)) {
        ImGui::End();
        return;
    }

    // Header: contador + refresh button + filtro futuro.
    if (m_issues.empty()) {
        ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f),
            "%s  %s", ICON_FA_CIRCLE_CHECK,
            I18n::T("editor.panel.asset_issues.empty").c_str());
    } else {
        ImGui::Text("%s  %zu %s",
                     ICON_FA_TRIANGLE_EXCLAMATION,
                     m_issues.size(),
                     I18n::T("editor.panel.asset_issues.count_suffix").c_str());
    }
    ImGui::SameLine();
    // El refresh real lo hace EditorApplication via requestRefresh+frame —
    // aca solo seteamos la flag; el orquestador lee y llama refresh.
    if (ImGui::Button(I18n::T("editor.panel.asset_issues.refresh").c_str())) {
        m_refreshRequested = true;
    }

    ImGui::Separator();

    if (m_issues.empty()) {
        ImGui::TextWrapped("%s",
            I18n::T("editor.panel.asset_issues.empty_hint").c_str());
        ImGui::End();
        return;
    }

    // Tabla: icon + tipo | detalle | path | usado por | accion.
    constexpr ImGuiTableFlags kFlags =
        ImGuiTableFlags_RowBg
        | ImGuiTableFlags_BordersInnerH
        | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("##asset_issues_table", 4, kFlags)) {
        ImGui::TableSetupColumn(
            I18n::T("editor.panel.asset_issues.col_kind").c_str(),
            ImGuiTableColumnFlags_WidthFixed, 28.0f);
        ImGui::TableSetupColumn(
            I18n::T("editor.panel.asset_issues.col_asset").c_str(),
            ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn(
            I18n::T("editor.panel.asset_issues.col_used_by").c_str(),
            ImGuiTableColumnFlags_WidthStretch, 0.35f);
        ImGui::TableSetupColumn(
            I18n::T("editor.panel.asset_issues.col_action").c_str(),
            ImGuiTableColumnFlags_WidthFixed, 90.0f);
        ImGui::TableHeadersRow();

        for (usize i = 0; i < m_issues.size(); ++i) {
            const auto& is = m_issues[i];
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();

            // Col 0: icon coloreado.
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, colorForKind(is.kind));
            ImGui::Text("%s", iconForKind(is.kind));
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s",
                    I18n::T(is.detail.c_str()).c_str());
            }

            // Col 1: path del asset + detail debajo.
            ImGui::TableSetColumnIndex(1);
            const std::string assetLabel = is.assetPath.empty()
                ? std::string{"(sin path)"} : is.assetPath;
            ImGui::TextWrapped("%s", assetLabel.c_str());
            ImGui::PushStyleColor(ImGuiCol_Text,
                ImVec4(0.65f, 0.65f, 0.65f, 1.0f));
            ImGui::TextWrapped("%s", I18n::T(is.detail.c_str()).c_str());
            ImGui::PopStyleColor();

            // Col 2: usado por.
            ImGui::TableSetColumnIndex(2);
            ImGui::TextWrapped("%s", is.usedBy.c_str());

            // Col 3: boton accion (go-to entity si aplica).
            ImGui::TableSetColumnIndex(3);
            const bool hasEntity = static_cast<bool>(is.entity);
            if (!hasEntity) ImGui::BeginDisabled();
            if (ImGui::Button(I18n::T("editor.panel.asset_issues.goto").c_str())) {
                m_pendingSelect = is.entity;
            }
            if (!hasEntity) {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::SetTooltip("%s",
                        I18n::T("editor.panel.asset_issues.goto_disabled")
                            .c_str());
                }
            }

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace Mood
