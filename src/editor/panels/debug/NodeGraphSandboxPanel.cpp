#include "editor/panels/debug/NodeGraphSandboxPanel.h"

#include "core/Log.h"
#include "editor/commands/HistoryStack.h"
#include "editor/commands/NodeGraphCommand.h"
#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"

#include <imgui.h>

#include <filesystem>
#include <fstream>

namespace Mood {

using namespace Mood::NodeGraph;

namespace {

// Path persistente para Save/Load del demo. Vive al lado del exe.
std::filesystem::path sandboxPath() {
    return std::filesystem::current_path() / "node_graph_sandbox.json";
}

} // namespace

// =============================================================
// Ctor
// =============================================================

NodeGraphSandboxPanel::NodeGraphSandboxPanel() {
    visible = false;  // off por default; toggle desde menu Ver
}

// =============================================================
// Render
// =============================================================

void NodeGraphSandboxPanel::onImGuiRender() {
    if (!visible) return;
    if (!m_initialized) {
        loadDemoGraph();
        m_initialized = true;
    }

    ImGui::SetNextWindowSize(ImVec2(900.0f, 600.0f), ImGuiCond_FirstUseEver);
    // F2H46: window ID estable = name() ("Node Graph Sandbox"). El
    // titulo visible es el mismo string en ingles — convencion con el
    // resto del editor (Inspector, Console, Performance no son i18n
    // en titulo de tab tampoco). El contenido SI es i18n.
    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    // --- Toolbar (cada button con ID estable via "##suffix" para que
    //     no colisione si la traduccion comparte texto con otro widget) ---
    if (ImGui::Button((I18n::T("editor.panel.nodegraph_sandbox.reset") + "##ngs_reset").c_str())) {
        m_graph.clear();
        loadDemoGraph();
        // Reset tambien limpia el HistoryStack (los commands pendientes
        // referencian nodos que ya no existen).
        if (m_ui) {
            if (HistoryStack* h = m_ui->historyStack()) h->clear();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((I18n::T("editor.panel.nodegraph_sandbox.save") + "##ngs_save").c_str())) {
        const auto path = sandboxPath();
        try {
            std::ofstream out(path);
            out << m_graph.toJson().dump(2);
            Log::editor()->info("[Sandbox] Saved {} nodes / {} links to {}",
                                  m_graph.nodeCount(), m_graph.linkCount(),
                                  path.generic_string());
        } catch (const std::exception& e) {
            Log::editor()->error("[Sandbox] Save fallo: {}", e.what());
        }
    }
    ImGui::SameLine();
    if (ImGui::Button((I18n::T("editor.panel.nodegraph_sandbox.load") + "##ngs_load").c_str())) {
        const auto path = sandboxPath();
        try {
            std::ifstream in(path);
            if (!in.is_open()) {
                Log::editor()->warn("[Sandbox] No existe {}", path.generic_string());
            } else {
                nlohmann::json j;
                in >> j;
                m_graph = Graph::fromJson(j);
                if (m_ui) {
                    if (HistoryStack* h = m_ui->historyStack()) h->clear();
                }
                Log::editor()->info("[Sandbox] Loaded {} nodes / {} links from {}",
                                      m_graph.nodeCount(), m_graph.linkCount(),
                                      path.generic_string());
            }
        } catch (const std::exception& e) {
            Log::editor()->error("[Sandbox] Load fallo: {}", e.what());
        }
    }
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    if (ImGui::Button((I18n::T("editor.panel.nodegraph_sandbox.add_source")  + "##ngs_addsrc").c_str())) addSourceNode({50.0f, 50.0f});
    ImGui::SameLine();
    if (ImGui::Button((I18n::T("editor.panel.nodegraph_sandbox.add_process") + "##ngs_addproc").c_str())) addProcessNode({200.0f, 50.0f});
    ImGui::SameLine();
    if (ImGui::Button((I18n::T("editor.panel.nodegraph_sandbox.add_sink")    + "##ngs_addsnk").c_str())) addSinkNode({400.0f, 50.0f});
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();
    if (ImGui::Button((I18n::T("editor.panel.nodegraph_sandbox.reset_view") + "##ngs_rstview").c_str())) m_editor.resetView();

    // --- Status ---
    ImGui::TextUnformatted(I18n::T("editor.panel.nodegraph_sandbox.status",
                                    m_graph.nodeCount(), m_graph.linkCount()).c_str());
    const auto sel = m_editor.selectedNodes();
    if (!sel.empty()) {
        ImGui::SameLine();
        ImGui::TextUnformatted(I18n::T("editor.panel.nodegraph_sandbox.selected").c_str());
        for (size_t i = 0; i < sel.size(); ++i) {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::Text("#%u%s", sel[i], (i + 1 < sel.size()) ? "," : "");
        }
    }
    ImGui::Separator();

    // --- Canvas ---
    // El warning de "conflicting ID" de ImGui 1.92 esta desactivado
    // globalmente en EditorApplication_Init.cpp (false-positive de
    // imgui-node-editor).
    ImGui::BeginChild("##NodeGraphCanvas",
                       ImVec2(0.0f, 0.0f),
                       false,
                       ImGuiWindowFlags_NoMove);
    const auto events = m_editor.draw(m_graph);
    ImGui::EndChild();

    applyEvents(events);

    ImGui::End();
}

// =============================================================
// Loaders
// =============================================================

void NodeGraphSandboxPanel::loadDemoGraph() {
    m_graph.clear();
    // Layout horizontal: source -> 2 process -> sink. Solo los nodos —
    // los links los hace el user para validar la creacion de links.
    // Los titulos se resuelven via i18n en el caller del Graph::Node::title;
    // aca guardamos las keys (el NodeGraphEditor las renderea via T() en
    // el futuro — v1 las guarda como literales traducidos al momento de
    // load para que tour-de-validacion vea texto en el idioma activo).
    const NodeId src = m_graph.addNode("source", glm::vec2(20.0f, 200.0f));
    m_graph.addSocket(src, SocketKind::Output, "flow", "out");
    if (Node* n = m_graph.findNode(src))
        n->title = I18n::T("editor.panel.nodegraph_sandbox.node_source");

    const NodeId p1 = m_graph.addNode("process", glm::vec2(220.0f, 100.0f));
    m_graph.addSocket(p1, SocketKind::Input,  "flow", "in");
    m_graph.addSocket(p1, SocketKind::Output, "flow", "out");
    if (Node* n = m_graph.findNode(p1))
        n->title = I18n::T("editor.panel.nodegraph_sandbox.node_process_a");

    const NodeId p2 = m_graph.addNode("process", glm::vec2(220.0f, 300.0f));
    m_graph.addSocket(p2, SocketKind::Input,  "flow", "in");
    m_graph.addSocket(p2, SocketKind::Output, "flow", "out");
    if (Node* n = m_graph.findNode(p2))
        n->title = I18n::T("editor.panel.nodegraph_sandbox.node_process_b");

    const NodeId snk = m_graph.addNode("sink", glm::vec2(420.0f, 200.0f));
    m_graph.addSocket(snk, SocketKind::Input, "flow", "in");
    if (Node* n = m_graph.findNode(snk))
        n->title = I18n::T("editor.panel.nodegraph_sandbox.node_sink");
}

// =============================================================
// Helpers de creacion (via command)
// =============================================================

void NodeGraphSandboxPanel::addProcessNode(glm::vec2 pos) {
    std::vector<AddNodeCommand::SocketSpec> ins  = {{"flow", "in"}};
    std::vector<AddNodeCommand::SocketSpec> outs = {{"flow", "out"}};
    auto cmd = std::make_unique<AddNodeCommand>(
        &m_graph, "process", pos, ins, outs,
        I18n::T("editor.panel.nodegraph_sandbox.node_process_a"),
        I18n::T("editor.panel.nodegraph_sandbox.cmd_add_process"));
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) { h->push(std::move(cmd)); return; }
    }
    cmd->execute();  // fallback sin undo
}

void NodeGraphSandboxPanel::addSourceNode(glm::vec2 pos) {
    std::vector<AddNodeCommand::SocketSpec> ins;
    std::vector<AddNodeCommand::SocketSpec> outs = {{"flow", "out"}};
    auto cmd = std::make_unique<AddNodeCommand>(
        &m_graph, "source", pos, ins, outs,
        I18n::T("editor.panel.nodegraph_sandbox.node_source"),
        I18n::T("editor.panel.nodegraph_sandbox.cmd_add_source"));
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) { h->push(std::move(cmd)); return; }
    }
    cmd->execute();
}

void NodeGraphSandboxPanel::addSinkNode(glm::vec2 pos) {
    std::vector<AddNodeCommand::SocketSpec> ins  = {{"flow", "in"}};
    std::vector<AddNodeCommand::SocketSpec> outs;
    auto cmd = std::make_unique<AddNodeCommand>(
        &m_graph, "sink", pos, ins, outs,
        I18n::T("editor.panel.nodegraph_sandbox.node_sink"),
        I18n::T("editor.panel.nodegraph_sandbox.cmd_add_sink"));
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) { h->push(std::move(cmd)); return; }
    }
    cmd->execute();
}

// =============================================================
// Event -> Command dispatch
// =============================================================

void NodeGraphSandboxPanel::applyEvents(const std::vector<EditorEvent>& events) {
    HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
    for (const EditorEvent& ev : events) {
        std::unique_ptr<ICommand> cmd;
        switch (ev.kind) {
            case EditorEvent::Kind::NodeMoved:
                cmd = std::make_unique<MoveNodeCommand>(
                    &m_graph, ev.nodeId, ev.oldPos, ev.newPos,
                    I18n::T("editor.panel.nodegraph_sandbox.cmd_move"));
                break;
            case EditorEvent::Kind::NodeDeleted:
                cmd = std::make_unique<RemoveNodeCommand>(
                    &m_graph, ev.nodeId,
                    I18n::T("editor.panel.nodegraph_sandbox.cmd_delete"));
                break;
            case EditorEvent::Kind::LinkCreated:
                cmd = std::make_unique<AddLinkCommand>(
                    &m_graph, ev.fromSocket, ev.toSocket,
                    I18n::T("editor.panel.nodegraph_sandbox.cmd_connect"));
                break;
            case EditorEvent::Kind::LinkDeleted:
                cmd = std::make_unique<RemoveLinkCommand>(
                    &m_graph, ev.linkId,
                    I18n::T("editor.panel.nodegraph_sandbox.cmd_disconnect"));
                break;
        }
        if (!cmd) continue;
        if (h) h->push(std::move(cmd));
        else   cmd->execute();
    }
}

} // namespace Mood
