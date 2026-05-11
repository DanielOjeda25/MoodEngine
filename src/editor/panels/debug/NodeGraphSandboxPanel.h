#pragma once

// NodeGraphSandboxPanel (F2H46 Bloque E): panel debug-only que ejercita
// el framework de NodeGraph (Graph + NodeGraphEditor + 5 commands).
//
// Proposito: validar visualmente que pan/zoom/sockets/conexiones/undo/
// save-load funcionan ANTES de que los editores reales (Dialog, Quest)
// los consuman. Tambien queda como utilidad permanente para futuros
// devs que extiendan el framework.
//
// Categoria "Debug". Default oculto. Toggle desde menu "Ver > Debug >
// Node Graph Sandbox".
//
// Contenido del panel:
// - Toolbar arriba: Reset (carga grafo demo), Save (a `node_graph_sandbox.json`
//   en `<exeDir>`), Load (mismo file), 2 botones para agregar nodos de
//   ejemplo (tipo "source" sin inputs / tipo "sink" sin outputs).
// - Canvas central: NodeGraphEditor sobre un grafo demo.
// - Status bar abajo: count de nodos + links + nodo seleccionado.

#include "editor/panels/IPanel.h"
#include "engine/nodegraph/Graph.h"
#include "engine/nodegraph/NodeGraphEditor.h"

namespace Mood {

class EditorUI;

class NodeGraphSandboxPanel : public IPanel {
public:
    NodeGraphSandboxPanel();

    void onImGuiRender() override;
    const char* name() const override { return "Node Graph Sandbox"; }
    const char* category() const override { return "Debug"; }

    /// @brief Inyecta el EditorUI para acceso al HistoryStack (las
    ///        mutaciones del grafo van via Command para undo/redo).
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    /// @brief Resetea el grafo a un demo de 4 nodos pre-armados +
    ///        algunos links. Util para "Reset" del toolbar y para
    ///        inicializar la primera vez.
    void loadDemoGraph();

    /// @brief Agrega un nodo "process" (1 input + 1 output) a la
    ///        posicion dada. Va por command (undoable).
    void addProcessNode(glm::vec2 position);

    /// @brief Agrega un nodo "source" (solo output) — util para
    ///        validar nodos sin inputs.
    void addSourceNode(glm::vec2 position);

    /// @brief Agrega un nodo "sink" (solo input) — util para validar
    ///        nodos sin outputs.
    void addSinkNode(glm::vec2 position);

    /// @brief Aplica todos los eventos retornados por draw() — cada
    ///        uno se convierte en Command y se pushea al HistoryStack.
    ///        Si no hay HistoryStack disponible, aplica directo (modo
    ///        degradado para soporte de tests).
    void applyEvents(const std::vector<NodeGraph::EditorEvent>& events);

    EditorUI*                m_ui = nullptr;
    NodeGraph::Graph         m_graph;
    NodeGraph::NodeGraphEditor m_editor;
    bool                     m_initialized = false;
};

} // namespace Mood
