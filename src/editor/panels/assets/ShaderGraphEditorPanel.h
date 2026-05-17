#pragma once

// ShaderGraphEditorPanel (F2H62 Bloque C): editor visual de un
// `Mood::ShaderGraph::Asset` abierto en el editor. Reusa
// `NodeGraphEditor` (F2H46) para el canvas, comparte commands con
// el DialogEditor (AddLink / RemoveLink / MoveNode / RemoveNode).
//
// Capacidades v1:
//   - Toolbar: New / Save / Save As / Close / Compile / Reset View.
//   - Canvas con pan/zoom (heredado del wrapper imgui-node-editor).
//   - Right-click sobre el canvas: popup "Add Node" con la palette de
//     20 NodeKinds.
//   - Inspector contextual del nodo seleccionado (lateral derecho):
//     edita literals de input pins desconectados + valores especificos
//     por kind (color value, float value, texture_path).
//   - Boton "Compilar" corre el generador GLSL y muestra warnings /
//     errores en una zona inferior. NO recompila el shader del runtime
//     todavia -- eso es Bloque E.
//
// El panel es own-the-asset (igual que DialogEditor): adopta el
// Asset cuando lo abre, lo guarda con saveToFile, y libera el state
// al cerrar.

#include "editor/panels/IPanel.h"
#include "engine/nodegraph/Graph.h"
#include "engine/nodegraph/NodeGraphEditor.h"
#include "engine/shader_graph/ShaderGraphAsset.h"
#include "engine/shader_graph/ShaderGraphGenerator.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace Mood {

class EditorUI;

class ShaderGraphEditorPanel : public IPanel {
public:
    ShaderGraphEditorPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Shader Graph"; }
    const char* category() const override { return "Assets"; }

    /// @brief Inyecta el EditorUI para acceder al HistoryStack.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    // ----- API publica (Browser / Welcome modal / Inspector lo usan) -----

    /// @brief Abre un asset desde un `.moodshader`. Si falla, loggea warn
    ///        y no muta state.
    void openFromFile(const std::filesystem::path& fsPath);

    /// @brief Adopta un asset ya construido + path. El caller es
    ///        responsable de chequear `dirty()` antes para no perder
    ///        cambios sin guardar del asset previo.
    void adoptAsset(ShaderGraph::Asset asset, std::filesystem::path fsPath);

    /// @brief Crea un asset nuevo en memoria con un OutputPBR ya
    ///        colocado a la derecha. Sin path asociado -- el primer
    ///        Save abrira "Save As".
    void newAsset();

    /// @brief Cierra el asset (sin save). Loggea warn si dirty.
    void closeAsset();

    /// @brief Guarda al path actual. False + log si no hay path.
    bool save();

    /// @brief Guarda a un path explicito. Update m_filePath.
    bool saveAs(const std::filesystem::path& fsPath);

    /// @brief Corre el generador GLSL y guarda el resultado en
    ///        m_lastCompile (para mostrar en la zona "Compile Output").
    void compile();

    // ----- Accessors -----

    ShaderGraph::Asset*       asset()       { return m_hasAsset ? &m_asset : nullptr; }
    const ShaderGraph::Asset* asset() const { return m_hasAsset ? &m_asset : nullptr; }
    bool hasAsset() const { return m_hasAsset; }
    bool dirty()    const { return m_dirty; }
    std::optional<std::filesystem::path> filePath() const { return m_filePath; }

    /// @brief Primer nodo seleccionado, k_invalidNodeId si nada.
    NodeGraph::NodeId selectedNode() const;

private:
    // UI helpers
    void drawToolbar();
    void drawCanvasAndPalette();
    void drawSelectionInspector();
    void drawCompileOutput();

    // Spawn / mutation
    void spawnNode(ShaderGraph::NodeKind kind, glm::vec2 canvasPos);

    // Procesa eventos del NodeGraphEditor (link create/delete, move,
    // delete). Pushea Commands al HistoryStack si esta disponible,
    // sino aplica directo.
    void applyEditorEvents(const std::vector<NodeGraph::EditorEvent>& events);

    // Helpers de socket-type (devuelven el typeTag string del socket).
    static const char* socketTypeOf(const NodeGraph::Node& node,
                                    NodeGraph::SocketId socketId);

    EditorUI*                              m_ui = nullptr;
    ShaderGraph::Asset                     m_asset;
    bool                                   m_hasAsset = false;
    bool                                   m_dirty = false;
    std::optional<std::filesystem::path>   m_filePath;
    NodeGraph::NodeGraphEditor             m_canvas;

    // Cache del ultimo compile() para que el dev pueda mantener abierta
    // la zona "Compile Output" mientras edita.
    ShaderGraph::GenResult                 m_lastCompile;
    bool                                   m_hasCompileResult = false;
};

} // namespace Mood
