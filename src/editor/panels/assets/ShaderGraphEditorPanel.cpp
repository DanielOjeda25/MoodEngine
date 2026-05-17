#include "editor/panels/assets/ShaderGraphEditorPanel.h"

#include "core/Log.h"
#include "editor/commands/HistoryStack.h"
#include "editor/commands/NodeGraphCommand.h"
#include "editor/ui/EditorUI.h"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace Mood {

using namespace Mood::NodeGraph;
using ShaderGraph::NodeKind;

// =============================================================
// Lifecycle de asset
// =============================================================

void ShaderGraphEditorPanel::openFromFile(const std::filesystem::path& fsPath) {
    auto loaded = ShaderGraph::Asset::loadFromFile(fsPath);
    if (!loaded.has_value()) {
        Log::editor()->warn("[ShaderGraphEditor] no pude cargar '{}'",
                              fsPath.generic_string());
        return;
    }
    adoptAsset(std::move(*loaded), fsPath);
}

void ShaderGraphEditorPanel::adoptAsset(ShaderGraph::Asset asset,
                                          std::filesystem::path fsPath) {
    m_asset    = std::move(asset);
    m_hasAsset = true;
    m_dirty    = false;
    m_filePath = std::move(fsPath);
    m_hasCompileResult = false;
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) h->clear();
    }
    visible = true;
    Log::editor()->info("[ShaderGraphEditor] abierto '{}' ({} nodos, {} links)",
                          m_filePath->generic_string(),
                          m_asset.graph().nodeCount(),
                          m_asset.graph().linkCount());
}

void ShaderGraphEditorPanel::newAsset() {
    m_asset = ShaderGraph::Asset{};
    m_asset.initEmpty();   // inserta un OutputPBR
    m_hasAsset = true;
    m_dirty    = true;     // sin guardar aun
    m_filePath.reset();
    m_hasCompileResult = false;
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) h->clear();
    }
    visible = true;
    Log::editor()->info("[ShaderGraphEditor] new asset en memoria");
}

void ShaderGraphEditorPanel::closeAsset() {
    if (m_dirty) {
        Log::editor()->warn("[ShaderGraphEditor] cerrando asset con cambios sin guardar");
    }
    m_asset    = ShaderGraph::Asset{};
    m_hasAsset = false;
    m_dirty    = false;
    m_filePath.reset();
    m_hasCompileResult = false;
    if (m_ui) {
        if (HistoryStack* h = m_ui->historyStack()) h->clear();
    }
}

bool ShaderGraphEditorPanel::save() {
    if (!m_hasAsset) return false;
    if (!m_filePath.has_value()) {
        Log::editor()->warn("[ShaderGraphEditor] save sin filepath — usar saveAs");
        return false;
    }
    if (!m_asset.saveToFile(*m_filePath)) return false;
    m_dirty = false;
    Log::editor()->info("[ShaderGraphEditor] guardado '{}'",
                          m_filePath->generic_string());
    return true;
}

bool ShaderGraphEditorPanel::saveAs(const std::filesystem::path& fsPath) {
    if (!m_hasAsset) return false;
    if (!m_asset.saveToFile(fsPath)) return false;
    m_filePath = fsPath;
    m_dirty = false;
    Log::editor()->info("[ShaderGraphEditor] guardado como '{}'",
                          fsPath.generic_string());
    return true;
}

void ShaderGraphEditorPanel::compile() {
    if (!m_hasAsset) return;
    const std::string tpl = ShaderGraph::loadDefaultTemplate();
    if (tpl.empty()) {
        Log::editor()->warn("[ShaderGraphEditor] plantilla GLSL no se pudo cargar");
        m_hasCompileResult = false;
        return;
    }
    m_lastCompile = ShaderGraph::generateGlsl(m_asset, tpl);
    m_hasCompileResult = true;
    const u32 warns = static_cast<u32>(
        m_lastCompile.messagesOf(ShaderGraph::GenSeverity::Warning).size());
    const u32 errs  = static_cast<u32>(
        m_lastCompile.messagesOf(ShaderGraph::GenSeverity::Error).size());
    Log::editor()->info(
        "[ShaderGraphEditor] compile {} ({} warnings, {} errors, {} chars GLSL)",
        m_lastCompile.succeeded ? "OK" : "FAIL",
        warns, errs, static_cast<u32>(m_lastCompile.glsl.size()));
}

NodeId ShaderGraphEditorPanel::selectedNode() const {
    const auto sel = m_canvas.selectedNodes();
    return sel.empty() ? k_invalidNodeId : sel.front();
}

// =============================================================
// Render
// =============================================================

void ShaderGraphEditorPanel::onImGuiRender() {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(1100.0f, 650.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(name(), &visible)) {
        ImGui::End();
        return;
    }

    if (!m_hasAsset) {
        ImGui::TextDisabled("Sin shader graph abierto");
        if (ImGui::Button("Nuevo shader graph")) {
            newAsset();
        }
        ImGui::End();
        return;
    }

    drawToolbar();
    ImGui::Separator();

    // Status line.
    ImGui::Text("Nodos: %u   Links: %u",
                static_cast<u32>(m_asset.graph().nodeCount()),
                static_cast<u32>(m_asset.graph().linkCount()));
    if (m_dirty) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.78f, 0.13f, 1.0f), " *");
    }
    if (m_filePath.has_value()) {
        ImGui::SameLine();
        ImGui::TextDisabled("  |  %s", m_filePath->filename().string().c_str());
    } else {
        ImGui::SameLine();
        ImGui::TextDisabled("  |  (sin guardar)");
    }
    if (m_asset.outputNodeId() == k_invalidNodeId) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                            "  |  FALTA NODO OutputPBR");
    }

    ImGui::Separator();

    // Layout: canvas (izq, ~75%) + sidebar inspector contextual (der, ~25%)
    const float contentW = ImGui::GetContentRegionAvail().x;
    const float sidebarW = std::clamp(contentW * 0.27f, 240.0f, 400.0f);
    const float canvasW  = contentW - sidebarW - 8.0f;

    ImGui::BeginChild("##ShaderCanvasArea", ImVec2(canvasW, 0.0f), true);
    drawCanvasAndPalette();
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("##ShaderInspector", ImVec2(sidebarW, 0.0f), true);
    drawSelectionInspector();
    ImGui::EndChild();

    drawCompileOutput();

    ImGui::End();
}

void ShaderGraphEditorPanel::drawToolbar() {
    if (ImGui::Button("Guardar")) {
        if (!m_filePath.has_value()) {
            Log::editor()->warn("[ShaderGraphEditor] Save sin filepath; usar Save As");
        } else {
            save();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Nuevo")) {
        if (m_dirty) {
            Log::editor()->warn("[ShaderGraphEditor] descartar cambios sin guardar al crear nuevo");
        }
        newAsset();
    }
    ImGui::SameLine();
    if (ImGui::Button("Compilar")) {
        compile();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset View")) {
        m_canvas.resetView();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cerrar")) {
        closeAsset();
    }
}

// -------------------------------------------------------------
// Canvas + palette
// -------------------------------------------------------------

void ShaderGraphEditorPanel::drawCanvasAndPalette() {
    // Hint visible para el usuario.
    ImGui::TextDisabled("Click derecho: agregar nodo  |  Del: borrar  |  drag socket → socket: conectar");
    ImGui::Separator();

    // Para que BeginPopupContextWindow capture el right-click del canvas,
    // tiene que ejecutarse DENTRO de la misma ImGui window que contiene
    // al canvas. La opcion mas robusta es chequear el right click sobre
    // el rect del child y abrir el popup manualmente.

    const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();

    ImGui::BeginChild("##GraphCanvas", canvasSize, false,
                       ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

    const auto events = m_canvas.draw(m_asset.graph());

    // Right-click sobre el child (siempre que NO sea sobre un nodo
    // -- imgui-node-editor maneja su propio popup encima). Aproximacion:
    // si el child fue clickeado con boton derecho y NO hay nodos
    // seleccionados, abrir el popup de palette.
    if (ImGui::IsWindowHovered() &&
        ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
        m_canvas.selectedNodes().empty()) {
        ImGui::OpenPopup("##AddNodePopup");
    }

    if (ImGui::BeginPopup("##AddNodePopup")) {
        ImGui::TextDisabled("Agregar nodo");
        ImGui::Separator();
        // Una lista plana ordenada por NodeKind. Para v2 podriamos
        // categorizar (Input / Math / Vector / Output) con submenus.
        for (NodeKind kind : ShaderGraph::allNodeKinds()) {
            if (ImGui::MenuItem(ShaderGraph::nodeKindDisplayName(kind))) {
                // Pos relativa al canvas + un offset arbitrario. v2:
                // mapear pos del mouse a coords del editor de nodos.
                const ImVec2 mouse = ImGui::GetMousePosOnOpeningCurrentPopup();
                const glm::vec2 pos{ mouse.x - canvasMin.x,
                                      mouse.y - canvasMin.y };
                spawnNode(kind, pos);
            }
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();

    applyEditorEvents(events);
}

void ShaderGraphEditorPanel::spawnNode(NodeKind kind, glm::vec2 canvasPos) {
    // v1: createNode directo en el asset. Para que sea undoable
    // tendriamos que pushear un AddNodeCommand con la spec completa
    // de sockets -- pero los sockets aca dependen del NodeKind, y
    // populateDefaultSockets no esta expuesto. Una opcion para v2:
    // wrappear createNode en un Command propio del shader graph.
    const NodeId id = m_asset.createNode(kind, canvasPos);
    if (id == k_invalidNodeId) {
        Log::editor()->warn("[ShaderGraphEditor] createNode fallo para kind={}",
                              static_cast<u32>(kind));
        return;
    }
    m_dirty = true;
    Log::editor()->info("[ShaderGraphEditor] add node id={} kind='{}'",
                          id, ShaderGraph::nodeKindToTypeTag(kind));
}

// -------------------------------------------------------------
// Inspector contextual
// -------------------------------------------------------------

const char* ShaderGraphEditorPanel::socketTypeOf(const Node& node, SocketId sid) {
    for (const Socket& s : node.inputs) {
        if (s.id == sid) return s.typeTag.c_str();
    }
    for (const Socket& s : node.outputs) {
        if (s.id == sid) return s.typeTag.c_str();
    }
    return "";
}

void ShaderGraphEditorPanel::drawSelectionInspector() {
    const NodeId sel = selectedNode();
    if (sel == k_invalidNodeId) {
        ImGui::TextDisabled("Inspector del nodo");
        ImGui::Separator();
        ImGui::TextWrapped("Seleccioná un nodo en el canvas para editar sus valores.");
        return;
    }
    Node* node = m_asset.graph().findNode(sel);
    if (!node) {
        ImGui::TextDisabled("Nodo invalido");
        return;
    }
    const auto kindOpt = ShaderGraph::nodeKindFromTypeTag(node->typeTag);

    ImGui::TextDisabled("Inspector del nodo");
    ImGui::Separator();
    ImGui::Text("ID: %u", node->id);
    ImGui::Text("Tipo: %s",
        kindOpt.has_value() ? ShaderGraph::nodeKindDisplayName(*kindOpt)
                            : node->typeTag.c_str());

    ImGui::Spacing();

    // ---- Editores especificos por NodeKind ----
    if (kindOpt.has_value()) {
        const NodeKind kind = *kindOpt;
        if (kind == NodeKind::Color) {
            glm::vec4 v = ShaderGraph::getColorValue(*node);
            float col[4] = {v.x, v.y, v.z, v.w};
            if (ImGui::ColorEdit4("##colorVal", col,
                    ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreview)) {
                ShaderGraph::setColorValue(*node, {col[0], col[1], col[2], col[3]});
                m_dirty = true;
            }
        } else if (kind == NodeKind::Float) {
            float f = ShaderGraph::getFloatValue(*node);
            if (ImGui::DragFloat("##floatVal", &f, 0.01f)) {
                ShaderGraph::setFloatValue(*node, f);
                m_dirty = true;
            }
        } else if (kind == NodeKind::SampleTexture) {
            std::string path = ShaderGraph::getTexturePath(*node);
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s", path.c_str());
            ImGui::Text("Textura:");
            if (ImGui::InputText("##texPath", buf, sizeof(buf))) {
                ShaderGraph::setTexturePath(*node, buf);
                m_dirty = true;
            }
            ImGui::TextDisabled("(drag textura del Asset Browser proximamente)");
        } else if (kind == NodeKind::OutputPBR) {
            ImGui::TextDisabled("Output PBR: edita los literals de los inputs abajo");
        }
    }

    ImGui::Spacing();
    if (!node->inputs.empty()) {
        ImGui::SeparatorText("Inputs");
        // Por cada input pin: si esta conectado → mostrar "(conectado)";
        // si no → editar el literal segun el typeTag.
        for (Socket& s : node->inputs) {
            const bool connected = m_asset.graph().findLinkByInput(s.id).has_value();
            ImGui::PushID(static_cast<int>(s.id));
            ImGui::TextUnformatted(s.name.empty() ? "(input)" : s.name.c_str());
            if (connected) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.40f, 0.78f, 0.40f, 1.0f),
                                    "  (conectado)");
                ImGui::PopID();
                continue;
            }

            glm::vec4 v = ShaderGraph::getSocketLiteral(*node, s.id);
            const std::string& tag = s.typeTag;
            bool changed = false;
            if (tag == ShaderGraph::kSocketType_Float) {
                float f = v.x;
                if (ImGui::DragFloat("##lit", &f, 0.01f)) {
                    v = {f, 0.0f, 0.0f, 0.0f};
                    changed = true;
                }
            } else if (tag == ShaderGraph::kSocketType_Vec2) {
                float xy[2] = {v.x, v.y};
                if (ImGui::DragFloat2("##lit", xy, 0.01f)) {
                    v = {xy[0], xy[1], 0.0f, 0.0f};
                    changed = true;
                }
            } else if (tag == ShaderGraph::kSocketType_Vec3) {
                // Si el nombre del input sugiere "color"/"albedo"/"emissive"
                // usamos ColorEdit3 (rango 0..1). Sino DragFloat3 libre.
                const bool isColorish = (s.name == "albedo" ||
                                            s.name == "emissive" ||
                                            s.name == "color");
                float xyz[3] = {v.x, v.y, v.z};
                if (isColorish) {
                    if (ImGui::ColorEdit3("##lit", xyz)) {
                        v = {xyz[0], xyz[1], xyz[2], 0.0f};
                        changed = true;
                    }
                } else {
                    if (ImGui::DragFloat3("##lit", xyz, 0.01f)) {
                        v = {xyz[0], xyz[1], xyz[2], 0.0f};
                        changed = true;
                    }
                }
            } else if (tag == ShaderGraph::kSocketType_Vec4) {
                float xyzw[4] = {v.x, v.y, v.z, v.w};
                if (ImGui::ColorEdit4("##lit", xyzw)) {
                    v = {xyzw[0], xyzw[1], xyzw[2], xyzw[3]};
                    changed = true;
                }
            } else {
                ImGui::TextDisabled("(tipo '%s')", tag.c_str());
            }

            if (changed) {
                ShaderGraph::setSocketLiteral(*node, s.id, v);
                m_dirty = true;
            }
            ImGui::PopID();
        }
    }
}

// -------------------------------------------------------------
// Compile output area
// -------------------------------------------------------------

void ShaderGraphEditorPanel::drawCompileOutput() {
    if (!m_hasCompileResult) return;

    ImGui::Separator();
    const ImVec4 col = m_lastCompile.succeeded
        ? ImVec4(0.13f, 0.83f, 0.43f, 1.0f)
        : ImVec4(1.0f, 0.30f, 0.30f, 1.0f);
    ImGui::TextColored(col, "Compilacion: %s — %u mensajes",
                         m_lastCompile.succeeded ? "OK" : "FAIL",
                         static_cast<u32>(m_lastCompile.messages.size()));

    if (m_lastCompile.messages.empty()) return;

    if (ImGui::BeginChild("##CompileMsgs", ImVec2(0.0f, 120.0f), true)) {
        for (const auto& msg : m_lastCompile.messages) {
            ImVec4 lineCol;
            const char* tag;
            switch (msg.severity) {
                case ShaderGraph::GenSeverity::Error:
                    lineCol = ImVec4(1.0f, 0.30f, 0.30f, 1.0f); tag = "[ERR ]"; break;
                case ShaderGraph::GenSeverity::Warning:
                    lineCol = ImVec4(1.0f, 0.78f, 0.13f, 1.0f); tag = "[WARN]"; break;
                default:
                    lineCol = ImVec4(0.70f, 0.70f, 0.70f, 1.0f); tag = "[INFO]"; break;
            }
            if (msg.nodeId != k_invalidNodeId) {
                ImGui::TextColored(lineCol, "%s node=%u: %s",
                                     tag, msg.nodeId, msg.text.c_str());
            } else {
                ImGui::TextColored(lineCol, "%s %s", tag, msg.text.c_str());
            }
        }
    }
    ImGui::EndChild();
}

// -------------------------------------------------------------
// Aplicar eventos del NodeGraphEditor → graph + history stack
// -------------------------------------------------------------

void ShaderGraphEditorPanel::applyEditorEvents(
    const std::vector<EditorEvent>& events) {
    if (events.empty()) return;
    HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
    Graph* g = &m_asset.graph();
    for (const EditorEvent& ev : events) {
        std::unique_ptr<ICommand> cmd;
        switch (ev.kind) {
            case EditorEvent::Kind::NodeMoved:
                cmd = std::make_unique<MoveNodeCommand>(
                    g, ev.nodeId, ev.oldPos, ev.newPos, "Mover nodo");
                break;
            case EditorEvent::Kind::NodeDeleted: {
                // Si era el OutputPBR, advertimos: el grafo queda sin
                // terminal y la compilacion fallara hasta que el dev
                // agregue uno nuevo.
                if (m_asset.outputNodeId() == ev.nodeId) {
                    Log::editor()->warn("[ShaderGraphEditor] OutputPBR borrado; "
                                          "agrega uno nuevo o la compilacion fallara");
                }
                cmd = std::make_unique<RemoveNodeCommand>(
                    g, ev.nodeId, "Borrar nodo");
                break;
            }
            case EditorEvent::Kind::LinkCreated:
                cmd = std::make_unique<AddLinkCommand>(
                    g, ev.fromSocket, ev.toSocket, "Conectar");
                break;
            case EditorEvent::Kind::LinkDeleted:
                cmd = std::make_unique<RemoveLinkCommand>(
                    g, ev.linkId, "Desconectar");
                break;
        }
        if (!cmd) continue;
        if (h) h->push(std::move(cmd));
        else   cmd->execute();
        m_dirty = true;
    }
}

} // namespace Mood
