#pragma once

// NodeGraphEditor (F2H46): wrapper visual sobre `imgui-node-editor`
// que consume un `Mood::NodeGraph::Graph` (modelo puro) y lo renderea
// como editor visual interactivo (pan/zoom/sockets/conexiones).
//
// Arquitectura:
// - pImpl para que clientes del header NO incluyan `imgui-node-editor.h`
//   ni ImGui. La unica dep visible es `Graph.h` (modelo puro).
// - draw() es no-mutating sobre el grafo: retorna una lista de
//   Event que el caller traduce a Command (HistoryStack) o aplica
//   directo. Esto desacopla el wrapper del sistema de undo/redo.
// - Una instancia = un editor (canvas independiente). Dialog Editor
//   tendra una, Quest Editor tendra otra, Material Editor pro otra.
//
// Lifecycle:
//   NodeGraphEditor ed;     // crea el ax::NodeEditor::EditorContext
//   ed.draw(graph);         // dentro de un ImGui::Begin/End
//   // ... aplicar events al graph
//   // ed destructor libera el contexto

#include "core/Types.h"
#include "engine/nodegraph/Graph.h"

#include <glm/vec2.hpp>

#include <memory>
#include <vector>

namespace Mood::NodeGraph {

/// @brief Evento emergente del editor visual. El caller los itera
///        despues de draw() para mutar el graph (directo o via
///        Command para undo/redo).
struct EditorEvent {
    enum class Kind : u8 {
        NodeMoved,    // un drag finalizo: oldPos -> newPos
        NodeDeleted,  // user presiono Del / context menu delete
        LinkCreated,  // user dibujo un link drag socket-to-socket
        LinkDeleted,  // user borro un link
    };
    Kind     kind        = Kind::NodeMoved;
    NodeId   nodeId      = k_invalidNodeId;
    LinkId   linkId      = k_invalidLinkId;
    SocketId fromSocket  = k_invalidSocketId;
    SocketId toSocket    = k_invalidSocketId;
    glm::vec2 oldPos     {0.0f, 0.0f};
    glm::vec2 newPos     {0.0f, 0.0f};
};

class NodeGraphEditor {
public:
    NodeGraphEditor();
    ~NodeGraphEditor();

    NodeGraphEditor(const NodeGraphEditor&) = delete;
    NodeGraphEditor& operator=(const NodeGraphEditor&) = delete;

    /// @brief Renderea el grafo dentro del ImGui window actual. Debe
    ///        llamarse entre ImGui::Begin/End. Procesa interacciones
    ///        (drag de nodos, drag para crear link, delete via teclado)
    ///        y retorna los eventos que el caller debe aplicar al graph.
    /// @param graph Grafo a visualizar. NO se muta dentro de draw()
    ///        — solo se lee. La lista de Event retornada describe los
    ///        cambios que el user pidio, pero aplicarlos es trabajo del
    ///        caller (asi puede envolverlos en Command para undo).
    /// @return Eventos a aplicar. Si vacio, no hubo cambios solicitados.
    std::vector<EditorEvent> draw(const Graph& graph);

    /// @brief IDs de los nodos seleccionados actualmente (selection
    ///        manejada internamente por la lib). Util para Inspector
    ///        contextual del nodo activo.
    std::vector<NodeId> selectedNodes() const;

    /// @brief Centra la vista en un nodo especifico (ej. al hacer
    ///        click en una entry del outline / search).
    void focusOnNode(NodeId id);

    /// @brief Resetea el viewport: vuelve al origen + zoom 1.0.
    void resetView();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace Mood::NodeGraph
