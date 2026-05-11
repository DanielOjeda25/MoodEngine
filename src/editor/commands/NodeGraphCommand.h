#pragma once

// NodeGraphCommand (F2H46 Bloque F): los 5 comandos undoable de
// mutacion de un `NodeGraph::Graph`.
//
// Convencion: el callsite (ej. Sandbox panel) NO muta el grafo
// directo — construye el Command y lo pushea al `HistoryStack`. El
// push() del stack llama execute() que aplica el cambio. Esto
// garantiza que TODA mutacion sea undoable.
//
// 5 operaciones:
//   AddNodeCommand     — crear nodo (con sockets pre-definidos).
//   RemoveNodeCommand  — borrar nodo + cascada de links incidentes.
//   MoveNodeCommand    — cambiar posicion (drag finalizado).
//   AddLinkCommand     — crear link entre 2 sockets.
//   RemoveLinkCommand  — borrar link.
//
// Caveat de IDs: addNode genera ID nuevo en execute() pero undo()
// borra ese ID. Si el user hace redo, execute() corre otra vez y
// genera un ID DISTINTO (porque Graph::m_nextNodeId siguio avanzando).
// Para que undo()/redo() sean simetricos perfectos, el comando
// recuerda el ID asignado en el primer execute() + re-utiliza la
// estructura "preserved" para el redo. Misma estrategia que
// DeleteEntityCommand del editor.

#include "core/Types.h"
#include "editor/commands/Command.h"
#include "engine/nodegraph/Graph.h"

#include <glm/vec2.hpp>

#include <string>

namespace Mood {

// ============================================================
// AddNodeCommand
// ============================================================

class AddNodeCommand : public ICommand {
public:
    /// @param graph Grafo a mutar (vive mas que el comando).
    /// @param typeTag Type del nodo a crear.
    /// @param position Pos inicial del nodo.
    /// @param inputs Sockets de entrada a crear (typeTag + name por socket).
    /// @param outputs Sockets de salida a crear.
    /// @param title Titulo opcional (i18n key o literal).
    /// @param label Nombre humano para el menu Editar (ej. "Add Dialog Line").
    struct SocketSpec {
        std::string typeTag;
        std::string name;
    };

    AddNodeCommand(NodeGraph::Graph* graph,
                    std::string typeTag,
                    glm::vec2 position,
                    std::vector<SocketSpec> inputs,
                    std::vector<SocketSpec> outputs,
                    std::string title = "",
                    std::string label = "Add Node");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

    /// ID del nodo creado (k_invalidNodeId hasta que execute() corre).
    NodeGraph::NodeId createdNodeId() const { return m_nodeId; }

private:
    NodeGraph::Graph*       m_graph;
    std::string             m_typeTag;
    glm::vec2               m_position;
    std::vector<SocketSpec> m_inputs;
    std::vector<SocketSpec> m_outputs;
    std::string             m_title;
    std::string             m_label;
    NodeGraph::NodeId       m_nodeId = NodeGraph::k_invalidNodeId;
};

// ============================================================
// RemoveNodeCommand
// ============================================================

class RemoveNodeCommand : public ICommand {
public:
    /// @param graph Grafo a mutar.
    /// @param nodeId Nodo a borrar.
    /// @param label Texto para el menu.
    RemoveNodeCommand(NodeGraph::Graph* graph,
                      NodeGraph::NodeId nodeId,
                      std::string label = "Remove Node");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    NodeGraph::Graph*           m_graph;
    NodeGraph::NodeId           m_nodeId;
    std::string                 m_label;
    // Snapshot pre-delete para reconstruir en undo. Capturado en ctor.
    NodeGraph::Node             m_savedNode;
    std::vector<NodeGraph::Link> m_savedIncidentLinks;
    bool                        m_captured = false;
};

// ============================================================
// MoveNodeCommand
// ============================================================

class MoveNodeCommand : public ICommand {
public:
    MoveNodeCommand(NodeGraph::Graph* graph,
                    NodeGraph::NodeId nodeId,
                    glm::vec2 oldPos,
                    glm::vec2 newPos,
                    std::string label = "Move Node");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    NodeGraph::Graph* m_graph;
    NodeGraph::NodeId m_nodeId;
    glm::vec2         m_oldPos;
    glm::vec2         m_newPos;
    std::string       m_label;
};

// ============================================================
// AddLinkCommand
// ============================================================

class AddLinkCommand : public ICommand {
public:
    AddLinkCommand(NodeGraph::Graph* graph,
                   NodeGraph::SocketId fromSocket,
                   NodeGraph::SocketId toSocket,
                   std::string label = "Connect");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

    NodeGraph::LinkId createdLinkId() const { return m_linkId; }

private:
    NodeGraph::Graph*   m_graph;
    NodeGraph::SocketId m_fromSocket;
    NodeGraph::SocketId m_toSocket;
    std::string         m_label;
    NodeGraph::LinkId   m_linkId = NodeGraph::k_invalidLinkId;
};

// ============================================================
// RemoveLinkCommand
// ============================================================

class RemoveLinkCommand : public ICommand {
public:
    RemoveLinkCommand(NodeGraph::Graph* graph,
                      NodeGraph::LinkId linkId,
                      std::string label = "Disconnect");

    void execute() override;
    void undo() override;
    std::string name() const override { return m_label; }

private:
    NodeGraph::Graph* m_graph;
    NodeGraph::LinkId m_linkId;
    std::string       m_label;
    // Snapshot pre-delete para reconstruir.
    NodeGraph::Link   m_savedLink;
    bool              m_captured = false;
};

} // namespace Mood
