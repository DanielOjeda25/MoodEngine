#include "editor/panels/narrative/DialogEditorPanel.h"

#include "core/Log.h"
#include "editor/commands/HistoryStack.h"
#include "editor/commands/NodeGraphCommand.h"
#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"

#include <imgui.h>

namespace Mood {

using namespace Mood::NodeGraph;
using Dialog::ValidationIssue;

// =============================================================
// Lifecycle de asset
// =============================================================

void DialogEditorPanel::openFromFile(const std::filesystem::path& fsPath) {
    auto loaded = Dialog::Asset::loadFromFile(fsPath);
    if (!loaded.has_value()) {
        Log::editor()->warn("[DialogEditor] no pude cargar '{}'",
                              fsPath.generic_string());
        return;
    }
    adoptAsset(std::move(*loaded), fsPath);
}

void DialogEditorPanel::adoptAsset(Dialog::Asset asset,
                                     std::filesystem::path fsPath) {
    m_asset    = std::move(asset);
    m_hasAsset = true;
    m_dirty    = false;
    m_filePath = std::move(fsPath);
    m_issues.clear();
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) h->clear();
    }
    visible = true;
    Log::editor()->info("[DialogEditor] abierto '{}' ({} nodos)",
                          m_filePath->generic_string(),
                          m_asset.graph().nodeCount());
}

void DialogEditorPanel::newAsset() {
    m_asset = Dialog::Asset{};
    m_asset.addLine(glm::vec2(100.0f, 100.0f));  // un nodo placeholder
    m_hasAsset = true;
    m_dirty    = true;   // sin guardar aun
    m_filePath.reset();
    m_issues.clear();
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) h->clear();
    }
    visible = true;
    Log::editor()->info("[DialogEditor] new asset en memoria");
}

void DialogEditorPanel::closeAsset() {
    if (m_dirty) {
        Log::editor()->warn("[DialogEditor] cerrando asset con cambios sin guardar");
    }
    m_asset    = Dialog::Asset{};
    m_hasAsset = false;
    m_dirty    = false;
    m_filePath.reset();
    m_issues.clear();
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) h->clear();
    }
}

bool DialogEditorPanel::save() {
    if (!m_hasAsset) return false;
    if (!m_filePath.has_value()) {
        Log::editor()->warn("[DialogEditor] save sin filepath — usar saveAs");
        return false;
    }
    if (!m_asset.saveToFile(*m_filePath)) return false;
    m_dirty = false;
    Log::editor()->info("[DialogEditor] guardado '{}'", m_filePath->generic_string());
    return true;
}

bool DialogEditorPanel::saveAs(const std::filesystem::path& fsPath) {
    if (!m_hasAsset) return false;
    if (!m_asset.saveToFile(fsPath)) return false;
    m_filePath = fsPath;
    m_dirty = false;
    Log::editor()->info("[DialogEditor] guardado como '{}'",
                          fsPath.generic_string());
    return true;
}

void DialogEditorPanel::setStartNodeFromSelection() {
    if (!m_hasAsset) return;
    const NodeId sel = selectedNode();
    if (sel == k_invalidNodeId) return;
    if (!m_asset.graph().findNode(sel)) return;
    m_asset.metadata().start_node_id = sel;
    m_dirty = true;
    Log::editor()->info("[DialogEditor] start_node = {}", sel);
}

std::vector<ValidationIssue> DialogEditorPanel::validate() {
    if (!m_hasAsset) return {};
    m_issues = Dialog::validate(m_asset);
    m_issuesPopupOpen = true;
    return m_issues;
}

NodeId DialogEditorPanel::selectedNode() const {
    const auto sel = m_canvas.selectedNodes();
    return sel.empty() ? k_invalidNodeId : sel.front();
}

void DialogEditorPanel::notifyLineEdited() {
    m_dirty = true;
}

// =============================================================
// Render
// =============================================================

void DialogEditorPanel::onImGuiRender() {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (!m_hasAsset) {
        ImGui::TextDisabled("%s", I18n::T("editor.panel.dialog_editor.no_asset").c_str());
        if (ImGui::Button(I18n::T("editor.panel.dialog_editor.new_asset").c_str())) {
            newAsset();
        }
        ImGui::End();
        return;
    }

    drawToolbar();
    ImGui::Separator();

    // Status line.
    const std::string status = I18n::T(
        "editor.panel.dialog_editor.status",
        m_asset.graph().nodeCount(),
        m_asset.graph().linkCount(),
        m_asset.metadata().start_node_id);
    ImGui::TextUnformatted(status.c_str());
    if (m_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.13f, 1.0f), " *");  // dot amarillo
    }
    if (m_filePath.has_value()) {
        ImGui::SameLine();
        ImGui::TextDisabled("  |  %s", m_filePath->filename().string().c_str());
    } else {
        ImGui::SameLine();
        ImGui::TextDisabled("  |  %s",
                              I18n::T("editor.panel.dialog_editor.unsaved").c_str());
    }

    ImGui::Separator();

    // Canvas.
    ImGui::BeginChild("##DialogCanvas", ImVec2(0.0f, 0.0f), false,
                       ImGuiWindowFlags_NoMove);
    const auto events = m_canvas.draw(m_asset.graph());
    ImGui::EndChild();

    applyEditorEvents(events);
    drawIssuesPopup();

    ImGui::End();
}

void DialogEditorPanel::drawToolbar() {
    if (ImGui::Button(I18n::T("editor.panel.dialog_editor.save").c_str())) {
        if (!m_filePath.has_value()) {
            Log::editor()->warn("[DialogEditor] Save sin filepath; usar Save As primero");
        } else {
            save();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.dialog_editor.add_line").c_str())) {
        m_asset.addLine(glm::vec2(50.0f, 50.0f));
        m_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.dialog_editor.set_start").c_str())) {
        setStartNodeFromSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.dialog_editor.validate").c_str())) {
        validate();
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.dialog_editor.reset_view").c_str())) {
        m_canvas.resetView();
    }
    ImGui::SameLine();
    if (ImGui::Button(I18n::T("editor.panel.dialog_editor.close").c_str())) {
        closeAsset();
    }
}

void DialogEditorPanel::drawIssuesPopup() {
    if (!m_issuesPopupOpen) return;
    if (!ImGui::Begin(I18n::T("editor.panel.dialog_editor.issues_title").c_str(),
                       &m_issuesPopupOpen)) {
        ImGui::End();
        return;
    }
    if (m_issues.empty()) {
        ImGui::TextColored(ImVec4(0.13f, 0.83f, 0.43f, 1.0f),
                            "%s", I18n::T("editor.panel.dialog_editor.no_issues").c_str());
    } else {
        for (const ValidationIssue& i : m_issues) {
            const ImVec4 col = (i.severity == ValidationIssue::Severity::Error)
                ? ImVec4(1.0f, 0.20f, 0.20f, 1.0f)
                : ImVec4(1.0f, 0.78f, 0.13f, 1.0f);
            const char* sevLabel = (i.severity == ValidationIssue::Severity::Error)
                ? "ERROR" : "WARN";
            ImGui::TextColored(col, "[%s] node=%u: %s",
                                sevLabel, i.nodeId, i.message.c_str());
        }
    }
    ImGui::End();
}

void DialogEditorPanel::applyEditorEvents(
    const std::vector<EditorEvent>& events) {
    if (events.empty()) return;
    HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
    Graph* g = &m_asset.graph();
    for (const EditorEvent& ev : events) {
        std::unique_ptr<ICommand> cmd;
        switch (ev.kind) {
            case EditorEvent::Kind::NodeMoved:
                cmd = std::make_unique<MoveNodeCommand>(
                    g, ev.nodeId, ev.oldPos, ev.newPos,
                    I18n::T("editor.panel.dialog_editor.cmd_move"));
                break;
            case EditorEvent::Kind::NodeDeleted:
                // Si el nodo borrado era el start, limpiamos metadata.
                if (m_asset.metadata().start_node_id == ev.nodeId) {
                    m_asset.metadata().start_node_id = k_invalidNodeId;
                    Log::editor()->warn("[DialogEditor] start_node fue borrado; "
                                          "marca otro con 'Set Start'");
                }
                cmd = std::make_unique<RemoveNodeCommand>(
                    g, ev.nodeId,
                    I18n::T("editor.panel.dialog_editor.cmd_delete"));
                break;
            case EditorEvent::Kind::LinkCreated:
                cmd = std::make_unique<AddLinkCommand>(
                    g, ev.fromSocket, ev.toSocket,
                    I18n::T("editor.panel.dialog_editor.cmd_connect"));
                break;
            case EditorEvent::Kind::LinkDeleted:
                cmd = std::make_unique<RemoveLinkCommand>(
                    g, ev.linkId,
                    I18n::T("editor.panel.dialog_editor.cmd_disconnect"));
                break;
        }
        if (!cmd) continue;
        if (h) h->push(std::move(cmd));
        else   cmd->execute();
        m_dirty = true;
    }
}

} // namespace Mood
