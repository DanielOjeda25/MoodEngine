#include "engine/nodegraph/NodeGraphEditor.h"

#include "core/Log.h"

#include <imgui.h>
#include <imgui_node_editor.h>

#include <unordered_map>

namespace ne = ax::NodeEditor;

namespace Mood::NodeGraph {

// =============================================================
// Helpers ID
// =============================================================

namespace {

inline ne::NodeId toLibNodeId(NodeId id)       { return ne::NodeId(static_cast<uintptr_t>(id)); }
inline ne::LinkId toLibLinkId(LinkId id)       { return ne::LinkId(static_cast<uintptr_t>(id)); }
inline ne::PinId  toLibPinId (SocketId id)     { return ne::PinId (static_cast<uintptr_t>(id)); }

inline NodeId   fromLibNodeId(ne::NodeId id) { return static_cast<NodeId>  (id.Get()); }
inline LinkId   fromLibLinkId(ne::LinkId id) { return static_cast<LinkId>  (id.Get()); }
inline SocketId fromLibPinId (ne::PinId  id) { return static_cast<SocketId>(id.Get()); }

inline glm::vec2 fromImVec2(const ImVec2& v) { return {v.x, v.y}; }
inline ImVec2    toImVec2  (const glm::vec2& v) { return {v.x, v.y}; }

} // namespace

// =============================================================
// pImpl
// =============================================================

struct NodeGraphEditor::Impl {
    ne::EditorContext* ctx = nullptr;

    // Tracking de IsActive() del frame anterior. Cuando pasa true->false
    // (drag termino), comparamos snapshot del start vs pos actual para
    // emitir NodeMoved con oldPos/newPos correcto.
    bool wasActive = false;

    // Snapshot de posiciones al INICIAR un drag (cuando IsActive() pasa
    // de false a true). Esto da el oldPos del evento NodeMoved.
    std::unordered_map<NodeId, glm::vec2> dragStartPositions;

    // Para focusOnNode: se aplica el siguiente frame antes de Begin().
    NodeId pendingFocus = k_invalidNodeId;
    bool   pendingReset = false;
};

// =============================================================
// Ctor / Dtor
// =============================================================

NodeGraphEditor::NodeGraphEditor() : m_impl(std::make_unique<Impl>()) {
    ne::Config cfg;
    cfg.SettingsFile = nullptr;  // persistencia la maneja nuestro Graph::toJson
    m_impl->ctx = ne::CreateEditor(&cfg);
}

NodeGraphEditor::~NodeGraphEditor() {
    if (m_impl && m_impl->ctx) {
        ne::DestroyEditor(m_impl->ctx);
        m_impl->ctx = nullptr;
    }
}

// =============================================================
// draw
// =============================================================

std::vector<EditorEvent> NodeGraphEditor::draw(const Graph& graph) {
    std::vector<EditorEvent> events;
    if (!m_impl || !m_impl->ctx) return events;

    ne::SetCurrentEditor(m_impl->ctx);

    // Pending viewport ops (de focusOnNode/resetView del frame previo).
    // La lib requiere estar dentro de Begin para aplicarlos — los
    // procesamos justo despues.

    ne::Begin("NodeGraphCanvas", ImVec2(0.0f, 0.0f));

    // --- Pending viewport ops ---
    if (m_impl->pendingReset) {
        ne::NavigateToContent(0.0f);  // 0.0 = instant, no animacion
        m_impl->pendingReset = false;
    }
    if (m_impl->pendingFocus != k_invalidNodeId) {
        if (graph.findNode(m_impl->pendingFocus)) {
            ne::SelectNode(toLibNodeId(m_impl->pendingFocus));
            ne::NavigateToSelection(false, 0.0f);
        }
        m_impl->pendingFocus = k_invalidNodeId;
    }

    // --- Sync graph -> lib de posiciones ---
    // Convencion: Graph es source of truth. La lib es solo "vista
    // mutable" durante un drag. Cuando NO hay drag activo, graph
    // gobierna: si las posiciones difieren (caso tipico: undo de
    // MoveNodeCommand revierte graph.position pero lib sigue en la
    // pos vieja), sincronizamos. Durante un drag (IsActive()=true),
    // la lib gobierna — el event handler escribira el nuevo valor en
    // graph al finalizar el drag.
    const bool dragActive = ne::IsActive();
    if (!dragActive) {
        for (const Node& n : graph.nodes()) {
            const glm::vec2 libPos = fromImVec2(
                ne::GetNodePosition(toLibNodeId(n.id)));
            if (libPos != n.position) {
                ne::SetNodePosition(toLibNodeId(n.id), toImVec2(n.position));
            }
        }
    }

    // --- Dibujar nodos ---
    for (const Node& n : graph.nodes()) {
        ne::BeginNode(toLibNodeId(n.id));

        // Header: titulo + typeTag chiquito abajo.
        if (!n.title.empty()) {
            ImGui::TextUnformatted(n.title.c_str());
        } else {
            ImGui::Text("Node #%u", n.id);
        }
        ImGui::TextDisabled("%s", n.typeTag.c_str());
        ImGui::Dummy(ImVec2(120.0f, 2.0f));  // ancho minimo del nodo

        // Sockets: inputs a la izquierda, outputs a la derecha.
        // Layout simple: stack vertical, una columna por kind.
        // Para v1 dibujamos primero todos los inputs, luego todos los
        // outputs (sin alinearlos lado a lado — eso es mejor con
        // ImGui::Tables, lo agregamos cuando emerja la necesidad).
        for (const Socket& s : n.inputs) {
            ne::BeginPin(toLibPinId(s.id), ne::PinKind::Input);
            ImGui::Text("> %s", s.name.empty() ? s.typeTag.c_str() : s.name.c_str());
            ne::EndPin();
        }
        for (const Socket& s : n.outputs) {
            ne::BeginPin(toLibPinId(s.id), ne::PinKind::Output);
            ImGui::Text("%s >", s.name.empty() ? s.typeTag.c_str() : s.name.c_str());
            ne::EndPin();
        }

        ne::EndNode();
    }

    // --- Dibujar links ---
    for (const Link& l : graph.links()) {
        ne::Link(toLibLinkId(l.id), toLibPinId(l.from), toLibPinId(l.to));
    }

    // --- Capturar interaccion: crear link ---
    if (ne::BeginCreate()) {
        ne::PinId startPin, endPin;
        if (ne::QueryNewLink(&startPin, &endPin)) {
            // La lib reporta start/end pero NO garantiza orden output->input.
            // Normalizamos: localizamos los sockets en el graph y decidimos.
            SocketId a = fromLibPinId(startPin);
            SocketId b = fromLibPinId(endPin);
            const Socket* sa = graph.findSocket(a);
            const Socket* sb = graph.findSocket(b);
            SocketId fromOutput = k_invalidSocketId;
            SocketId toInput    = k_invalidSocketId;
            if (sa && sb) {
                if (sa->kind == SocketKind::Output && sb->kind == SocketKind::Input) {
                    fromOutput = a; toInput = b;
                } else if (sb->kind == SocketKind::Output && sa->kind == SocketKind::Input) {
                    fromOutput = b; toInput = a;
                }
            }
            if (fromOutput != k_invalidSocketId &&
                graph.canConnect(fromOutput, toInput)) {
                if (ne::AcceptNewItem()) {
                    EditorEvent ev;
                    ev.kind = EditorEvent::Kind::LinkCreated;
                    ev.fromSocket = fromOutput;
                    ev.toSocket   = toInput;
                    events.push_back(ev);
                }
            } else {
                ne::RejectNewItem();
            }
        }
    }
    ne::EndCreate();

    // --- Capturar interaccion: delete node o link ---
    if (ne::BeginDelete()) {
        ne::NodeId deletedNode;
        while (ne::QueryDeletedNode(&deletedNode)) {
            if (ne::AcceptDeletedItem()) {
                EditorEvent ev;
                ev.kind   = EditorEvent::Kind::NodeDeleted;
                ev.nodeId = fromLibNodeId(deletedNode);
                events.push_back(ev);
            }
        }
        ne::LinkId deletedLink;
        while (ne::QueryDeletedLink(&deletedLink)) {
            if (ne::AcceptDeletedItem()) {
                EditorEvent ev;
                ev.kind   = EditorEvent::Kind::LinkDeleted;
                ev.linkId = fromLibLinkId(deletedLink);
                events.push_back(ev);
            }
        }
    }
    ne::EndDelete();

    // --- Detectar drag de nodos finalizado ---
    // Estrategia: trackear IsActive(). Cuando pasa de true a false
    // (drag termino), comparamos posiciones actuales vs las del drag
    // start para emitir NodeMoved con oldPos/newPos correcto.
    const bool nowActive = dragActive;  // ya leido arriba
    if (nowActive && !m_impl->wasActive) {
        // Arranco el drag — snapshot de todas las pos actuales para
        // poder retornar el oldPos correcto al soltar.
        m_impl->dragStartPositions.clear();
        for (const Node& n : graph.nodes()) {
            m_impl->dragStartPositions[n.id] =
                fromImVec2(ne::GetNodePosition(toLibNodeId(n.id)));
        }
    } else if (!nowActive && m_impl->wasActive) {
        // Termino el drag — emitir NodeMoved para los que cambiaron.
        for (const Node& n : graph.nodes()) {
            const glm::vec2 cur = fromImVec2(ne::GetNodePosition(toLibNodeId(n.id)));
            const auto it = m_impl->dragStartPositions.find(n.id);
            if (it != m_impl->dragStartPositions.end()) {
                const glm::vec2 old = it->second;
                if (cur != old) {
                    EditorEvent ev;
                    ev.kind   = EditorEvent::Kind::NodeMoved;
                    ev.nodeId = n.id;
                    ev.oldPos = old;
                    ev.newPos = cur;
                    events.push_back(ev);
                }
            }
        }
        m_impl->dragStartPositions.clear();
    }
    m_impl->wasActive = nowActive;

    ne::End();
    ne::SetCurrentEditor(nullptr);

    return events;
}

// =============================================================
// Selection
// =============================================================

std::vector<NodeId> NodeGraphEditor::selectedNodes() const {
    std::vector<NodeId> result;
    if (!m_impl || !m_impl->ctx) return result;
    ne::SetCurrentEditor(m_impl->ctx);
    const int count = ne::GetSelectedObjectCount();
    if (count <= 0) {
        ne::SetCurrentEditor(nullptr);
        return result;
    }
    std::vector<ne::NodeId> libIds(count);
    const int got = ne::GetSelectedNodes(libIds.data(), count);
    result.reserve(got);
    for (int i = 0; i < got; ++i) result.push_back(fromLibNodeId(libIds[i]));
    ne::SetCurrentEditor(nullptr);
    return result;
}

// =============================================================
// Viewport ops (aplicados el proximo frame por draw)
// =============================================================

void NodeGraphEditor::focusOnNode(NodeId id) {
    if (m_impl) m_impl->pendingFocus = id;
}

void NodeGraphEditor::resetView() {
    if (m_impl) m_impl->pendingReset = true;
}

} // namespace Mood::NodeGraph
