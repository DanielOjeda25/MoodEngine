#include "engine/nodegraph/NodeGraphEditor.h"

#include "core/Log.h"

#include <imgui.h>
#include <imnodes.h>

#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace Mood::NodeGraph {

// =============================================================
// Init global de imnodes
// =============================================================
//
// imnodes tiene UN contexto global (ImNodes::CreateContext / DestroyContext)
// MAS un EditorContext por cada canvas. El global hay que crearlo UNA vez
// despues de ImGui::CreateContext() y antes del primer draw. Lazy init
// con un flag atomico: el primer NodeGraphEditor que se construya dispara
// el create global. El destroy lo deja para el SO al cerrar el proceso
// (no es leak relevante para una app desktop con un solo proceso).

namespace {

std::atomic<bool> g_imnodesInitialized{false};

void ensureGlobalImnodesContext() {
    bool expected = false;
    if (g_imnodesInitialized.compare_exchange_strong(expected, true)) {
        ImNodes::CreateContext();
        // Estilo: el pin se renderea como circulo lleno, mismo color
        // que en nuestra version anterior. Snap suave de links.
        ImNodes::PushAttributeFlag(ImNodesAttributeFlags_EnableLinkDetachWithDragClick);
    }
}

// imnodes usa `int` para IDs. Como nuestros u32 caben siempre en int
// positivo para el rango que manejamos (millones), el cast es seguro.
inline int   toLibId   (u32 id)  { return static_cast<int>(id); }
inline u32   fromLibId (int id)  { return static_cast<u32>(id); }

inline ImVec2 toImVec2  (const glm::vec2& v) { return {v.x, v.y}; }
inline glm::vec2 fromImVec2(const ImVec2& v) { return {v.x, v.y}; }

} // namespace

// =============================================================
// pImpl
// =============================================================

struct NodeGraphEditor::Impl {
    ImNodesEditorContext* ctx = nullptr;

    // Snapshot de posiciones del frame anterior para detectar drag
    // finalizado. imnodes no provee un evento "node moved"; el wrapper
    // diff'ea cada frame y emite un EditorEvent cuando hay cambio.
    // Para no spammear durante el drag activo, solo emitimos cuando
    // el mouse boton 1 ESTABA presionado y ya NO lo esta (drag suelto).
    std::unordered_map<NodeId, glm::vec2> lastFramePositions;
    bool mouseLeftDownLastFrame = false;
    // Pos al inicio del drag (capturada en el primer frame con mouse
    // down sobre un nodo). Usada como oldPos del NodeMoved.
    std::unordered_map<NodeId, glm::vec2> dragStartPositions;

    // Para focusOnNode / resetView: aplicado en el proximo draw().
    NodeId pendingFocus = k_invalidNodeId;
    bool   pendingReset = false;

    // Track de links que el caller registro este frame -> link.id ->
    // (from, to). Usado para resolver IsLinkDestroyed (imnodes solo
    // devuelve el link id, no los endpoints, asi que necesitamos
    // mantener el lookup local).
    // No usado aun pero util si en el futuro queremos resolver.

    // Para evitar repetir SetNodeGridSpacePos en cada frame: imnodes
    // recuerda la pos en su state file (memoria). Solo seteamos cuando
    // el graph cambio la pos por afuera (undo de MoveNodeCommand) o al
    // primer draw de un nodo nuevo.
    std::unordered_set<NodeId> seenNodes;

    // Pos grid-space del ultimo right-click sobre el background.
    // Capturado durante draw(), consumido por el callback del popup
    // (el popup puede vivir multiples frames).
    glm::vec2 lastBgMenuGridPos{0.0f, 0.0f};
};

// =============================================================
// Ctor / Dtor
// =============================================================

NodeGraphEditor::NodeGraphEditor() : m_impl(std::make_unique<Impl>()) {
    ensureGlobalImnodesContext();
    m_impl->ctx = ImNodes::EditorContextCreate();
}

NodeGraphEditor::~NodeGraphEditor() {
    if (m_impl && m_impl->ctx) {
        ImNodes::EditorContextFree(m_impl->ctx);
        m_impl->ctx = nullptr;
    }
}

// =============================================================
// draw
// =============================================================

std::vector<EditorEvent> NodeGraphEditor::draw(const Graph& graph,
                                                  const EditorDrawConfig* cfg) {
    std::vector<EditorEvent> events;
    if (!m_impl || !m_impl->ctx) return events;

    ImNodes::EditorContextSet(m_impl->ctx);

    // --- Pending viewport ops ---
    // Aplicado antes del Begin para que tenga efecto en el frame.
    if (m_impl->pendingReset) {
        ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
        m_impl->pendingReset = false;
    }
    if (m_impl->pendingFocus != k_invalidNodeId) {
        const NodeId fid = m_impl->pendingFocus;
        if (graph.findNode(fid)) {
            // imnodes no tiene "navigate to selection"; lo simulamos:
            // pan tal que el nodo quede aprox en el centro del canvas.
            // Aproximacion simple — el panel puede mejorarla a futuro.
            ImNodes::EditorContextMoveToNode(toLibId(fid));
        }
        m_impl->pendingFocus = k_invalidNodeId;
    }

    ImNodes::BeginNodeEditor();

    // --- Dibujar nodos ---
    for (const Node& n : graph.nodes()) {
        // Sync graph -> imnodes: la primera vez que vemos el nodo, o si
        // el graph cambio la posicion por afuera (undo de Move), forzamos
        // SetNodeGridSpacePos. Despues imnodes mantiene su estado.
        const bool firstTimeSeen =
            m_impl->seenNodes.find(n.id) == m_impl->seenNodes.end();
        if (firstTimeSeen) {
            ImNodes::SetNodeGridSpacePos(toLibId(n.id), toImVec2(n.position));
            m_impl->seenNodes.insert(n.id);
        } else {
            // Si el graph movio el nodo por afuera (undo/redo), resync.
            // CRITICO: no resync durante drag (imnodes gobierna la pos)
            // NI en el frame en que el drag acaba de terminar (en ese
            // frame imnodes ya tiene la newPos pero graph todavia tiene
            // oldPos -- el caller aplica MoveNodeCommand DESPUES del
            // draw, asi que aca el resync sobreescribiria newPos a
            // oldPos rompiendo el move). Solo resync cuando el mouse
            // estuvo idle en este frame Y el anterior.
            const bool mouseIdle = !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
                                    !m_impl->mouseLeftDownLastFrame;
            if (mouseIdle) {
                const ImVec2 libPos = ImNodes::GetNodeGridSpacePos(toLibId(n.id));
                const glm::vec2 libGlm = fromImVec2(libPos);
                if (libGlm != n.position) {
                    ImNodes::SetNodeGridSpacePos(toLibId(n.id),
                                                    toImVec2(n.position));
                }
            }
        }

        ImNodes::BeginNode(toLibId(n.id));

        ImNodes::BeginNodeTitleBar();
        if (!n.title.empty()) {
            // title ya es semantico (ej. "Color", "Output PBR"); con
            // eso alcanza. NO mostramos typeTag debajo para no
            // duplicar info.
            ImGui::TextUnformatted(n.title.c_str());
        } else {
            // Sin title seteado: fallback "Node #N" + typeTag debajo
            // como info (caso editores legacy que no setean title).
            ImGui::Text("Node #%u", n.id);
        }
        ImNodes::EndNodeTitleBar();
        if (n.title.empty()) {
            ImGui::TextDisabled("%s", n.typeTag.c_str());
        }

        // Sockets: imnodes maneja el pin como circulito a la izquierda
        // (Input) o derecha (Output) automaticamente. El label va
        // adentro -- imnodes solo lo trata como widget interno, NO
        // como hot zone del pin. El cuerpo del nodo arrastra desde
        // cualquier punto.
        for (const Socket& s : n.inputs) {
            ImNodes::BeginInputAttribute(toLibId(s.id),
                                            ImNodesPinShape_CircleFilled);
            ImGui::TextUnformatted(s.name.empty() ? s.typeTag.c_str()
                                                      : s.name.c_str());
            ImNodes::EndInputAttribute();
        }
        for (const Socket& s : n.outputs) {
            ImNodes::BeginOutputAttribute(toLibId(s.id),
                                             ImNodesPinShape_CircleFilled);
            ImGui::TextUnformatted(s.name.empty() ? s.typeTag.c_str()
                                                      : s.name.c_str());
            ImNodes::EndOutputAttribute();
        }

        ImNodes::EndNode();
    }

    // --- Dibujar links ---
    for (const Link& l : graph.links()) {
        ImNodes::Link(toLibId(l.id), toLibId(l.from), toLibId(l.to));
    }

    // --- Background context menu (hook del caller) ---
    // imnodes no tiene Suspend/Resume; los popups van afuera de
    // BeginNodeEditor/EndNodeEditor pero el detect del right-click
    // sobre el background se hace ANTES del End (mientras el canvas
    // esta hovered). Capturamos aca y abrimos el popup despues del End.
    bool requestOpenBgMenu = false;
    if (cfg && cfg->drawBackgroundContextMenu) {
        if (ImNodes::IsEditorHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
            !ImNodes::IsAnyAttributeActive()) {
            requestOpenBgMenu = true;
            // Capturamos pos en pantalla; despues del End la convertimos
            // a grid space con la API de imnodes.
        }
    }

    ImNodes::EndNodeEditor();

    // --- Popup de background context menu ---
    // imnodes no expone screen->grid conversion publica. Calculamos
    // el offset usando un nodo del graph como referencia: el delta entre
    // su pos screen y su pos grid es exactamente el origen del canvas
    // en pantalla. Si no hay nodos todavia (improbable: initEmpty crea
    // un OutputPBR), fallback a {0,0}.
    auto screenToGridPos = [&](ImVec2 screen) -> glm::vec2 {
        if (!graph.nodes().empty()) {
            const NodeId ref = graph.nodes().front().id;
            const ImVec2 refScreen = ImNodes::GetNodeScreenSpacePos(toLibId(ref));
            const ImVec2 refGrid   = ImNodes::GetNodeGridSpacePos  (toLibId(ref));
            return {
                screen.x - (refScreen.x - refGrid.x),
                screen.y - (refScreen.y - refGrid.y),
            };
        }
        return {0.0f, 0.0f};
    };

    static constexpr const char* k_bgMenuPopupId = "##NGE_BgContextMenu";
    if (requestOpenBgMenu) {
        m_impl->lastBgMenuGridPos = screenToGridPos(ImGui::GetMousePos());
        ImGui::OpenPopup(k_bgMenuPopupId);
    }
    if (cfg && cfg->drawBackgroundContextMenu) {
        if (ImGui::BeginPopup(k_bgMenuPopupId)) {
            cfg->drawBackgroundContextMenu(m_impl->lastBgMenuGridPos);
            ImGui::EndPopup();
        }
    }

    // --- Capturar eventos de imnodes (post-End) ---

    // Link creado: imnodes garantiza orden output->input via
    // IsLinkCreated(start_attr_id, end_attr_id) donde start es siempre
    // del lado output. PERO igual normalizamos con el graph.
    int startAttr = 0, endAttr = 0;
    if (ImNodes::IsLinkCreated(&startAttr, &endAttr)) {
        SocketId a = fromLibId(startAttr);
        SocketId b = fromLibId(endAttr);
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
            EditorEvent ev;
            ev.kind = EditorEvent::Kind::LinkCreated;
            ev.fromSocket = fromOutput;
            ev.toSocket   = toInput;
            events.push_back(ev);
        }
    }

    // Link destruido (drag-detach o backspace sobre link seleccionado).
    int destroyedLinkId = 0;
    if (ImNodes::IsLinkDestroyed(&destroyedLinkId)) {
        EditorEvent ev;
        ev.kind = EditorEvent::Kind::LinkDeleted;
        ev.linkId = fromLibId(destroyedLinkId);
        events.push_back(ev);
    }

    // Delete key sobre nodos/links seleccionados. imnodes NO maneja
    // esto automaticamente -- el wrapper lo provee como "comportamiento
    // estandar" para que el caller no tenga que duplicar logica.
    // Condicion: la window que contiene el editor tiene el focus
    // (suficiente -- IsEditorHovered exigia el mouse encima del canvas,
    // demasiado restrictivo: el dev presiona Del con el cursor en
    // cualquier lado).
    const bool delPressed = ImGui::IsKeyPressed(ImGuiKey_Delete) ||
                              ImGui::IsKeyPressed(ImGuiKey_Backspace);
    const bool focusOk = ImGui::IsWindowFocused(
        ImGuiFocusedFlags_RootAndChildWindows);
    if (delPressed && focusOk) {
        const int nodeSelCount = ImNodes::NumSelectedNodes();
        if (nodeSelCount > 0) {
            std::vector<int> ids(nodeSelCount);
            ImNodes::GetSelectedNodes(ids.data());
            for (int id : ids) {
                EditorEvent ev;
                ev.kind = EditorEvent::Kind::NodeDeleted;
                ev.nodeId = fromLibId(id);
                events.push_back(ev);
            }
        }
        const int linkSelCount = ImNodes::NumSelectedLinks();
        if (linkSelCount > 0) {
            std::vector<int> ids(linkSelCount);
            ImNodes::GetSelectedLinks(ids.data());
            for (int id : ids) {
                EditorEvent ev;
                ev.kind = EditorEvent::Kind::LinkDeleted;
                ev.linkId = fromLibId(id);
                events.push_back(ev);
            }
        }
    }

    // --- Detectar drag de nodos finalizado ---
    // Estrategia: trackeamos pos del frame anterior + state del mouse.
    // Cuando el mouse pasa de pressed -> released, comparamos pos
    // actuales vs dragStart y emitimos NodeMoved si cambiaron.
    const bool mouseDownNow = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if (mouseDownNow && !m_impl->mouseLeftDownLastFrame) {
        // Inicio de potencial drag: snapshot de todas las pos.
        m_impl->dragStartPositions.clear();
        for (const Node& n : graph.nodes()) {
            const ImVec2 p = ImNodes::GetNodeGridSpacePos(toLibId(n.id));
            m_impl->dragStartPositions[n.id] = fromImVec2(p);
        }
    } else if (!mouseDownNow && m_impl->mouseLeftDownLastFrame) {
        // Fin de drag: emitir NodeMoved para los que cambiaron.
        for (const Node& n : graph.nodes()) {
            const auto it = m_impl->dragStartPositions.find(n.id);
            if (it == m_impl->dragStartPositions.end()) continue;
            const glm::vec2 old = it->second;
            const ImVec2 curIm = ImNodes::GetNodeGridSpacePos(toLibId(n.id));
            const glm::vec2 cur = fromImVec2(curIm);
            if (cur != old) {
                EditorEvent ev;
                ev.kind   = EditorEvent::Kind::NodeMoved;
                ev.nodeId = n.id;
                ev.oldPos = old;
                ev.newPos = cur;
                events.push_back(ev);
            }
        }
        m_impl->dragStartPositions.clear();
    }
    m_impl->mouseLeftDownLastFrame = mouseDownNow;

    return events;
}

// =============================================================
// Selection
// =============================================================

std::vector<NodeId> NodeGraphEditor::selectedNodes() const {
    std::vector<NodeId> result;
    if (!m_impl || !m_impl->ctx) return result;
    ImNodes::EditorContextSet(m_impl->ctx);
    const int count = ImNodes::NumSelectedNodes();
    if (count <= 0) return result;
    std::vector<int> libIds(count);
    ImNodes::GetSelectedNodes(libIds.data());
    result.reserve(count);
    for (int i = 0; i < count; ++i) result.push_back(fromLibId(libIds[i]));
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
