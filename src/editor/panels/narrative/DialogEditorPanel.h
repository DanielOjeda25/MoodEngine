#pragma once

// DialogEditorPanel (F2H47): editor visual de un `Mood::Dialog::Asset`
// abierto en el editor. Reusa `NodeGraphEditor` (F2H46) para el canvas;
// agrega toolbar (Save/Save As/Close/Set Start/Validate), file path
// tracking, dirty flag, integracion con HistoryStack via los
// NodeGraphCommand existentes.
//
// Owns el Asset cargado. Cuando el dev selecciona un nodo, el
// DialogNodeInspectorPanel consulta esta panel para leer/escribir el
// customData.
//
// Es categoria "Narrative" para que solo aparezca en el workspace
// "Narrativa" cuando un dialog esta abierto.

#include "editor/panels/IPanel.h"
#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogValidator.h"
#include "engine/nodegraph/Graph.h"
#include "engine/nodegraph/NodeGraphEditor.h"

#include <filesystem>
#include <optional>
#include <vector>

namespace Mood {

class EditorUI;

class DialogEditorPanel : public IPanel {
public:
    DialogEditorPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Dialog Editor"; }
    const char* category() const override { return "Narrative"; }

    /// @brief Inyecta el EditorUI para acceso al HistoryStack.
    void setEditorUi(EditorUI* ui) { m_ui = ui; }

    // ----- API publica (DialogBrowserPanel / Welcome modal lo usan) -----

    /// @brief Abre un asset desde un .mooddialog en disco. Si falla
    ///        (no existe / parse error), loggea warn y queda sin
    ///        cambios.
    void openFromFile(const std::filesystem::path& fsPath);

    /// @brief Adopta un asset ya construido + path. El caller es
    ///        responsable de chequear `dirty()` antes para no perder
    ///        cambios sin guardar del asset previo.
    void adoptAsset(Dialog::Asset asset, std::filesystem::path fsPath);

    /// @brief Crea un asset nuevo vacio en memoria (sin path asociado
    ///        — el primer Save abrira "Save As" para pedir path).
    void newAsset();

    /// @brief Cierra el asset actual (sin save). Si dirty, loggea warn
    ///        — el caller del UI tiene que pedir confirmacion.
    void closeAsset();

    /// @brief Guarda al path actual. Si no hay path, devuelve false +
    ///        log (el UI debe llamar saveAs).
    bool save();

    /// @brief Guarda a un path explicito. Update m_filePath.
    bool saveAs(const std::filesystem::path& fsPath);

    /// @brief Marca el asset actual como `start_node_id`. Si no hay
    ///        nodo seleccionado o el id no existe, no hace nada.
    void setStartNodeFromSelection();

    /// @brief Corre el validator y devuelve los issues. Tambien los
    ///        guarda en m_issues para render del panel.
    std::vector<Dialog::ValidationIssue> validate();

    // ----- Accessors para Inspector contextual -----

    /// @brief El asset actualmente abierto. nullptr si no hay.
    Dialog::Asset* asset() { return m_hasAsset ? &m_asset : nullptr; }
    const Dialog::Asset* asset() const { return m_hasAsset ? &m_asset : nullptr; }

    /// @brief Primer nodo seleccionado (o k_invalidNodeId si nada).
    NodeGraph::NodeId selectedNode() const;

    /// @brief El Inspector llama esto despues de editar un Line para
    ///        marcar el asset como dirty + sincronizar sockets.
    void notifyLineEdited();

    /// @brief Path del archivo asociado (vacio si no se guardo aun).
    std::optional<std::filesystem::path> filePath() const { return m_filePath; }

    /// @brief Hay cambios sin guardar.
    bool dirty() const { return m_dirty; }

    /// @brief Hay asset cargado en memoria.
    bool hasAsset() const { return m_hasAsset; }

private:
    void drawToolbar();
    void drawIssuesPopup();
    void applyEditorEvents(const std::vector<NodeGraph::EditorEvent>& events);

    EditorUI*                              m_ui = nullptr;
    Dialog::Asset                          m_asset;
    bool                                   m_hasAsset = false;
    bool                                   m_dirty = false;
    std::optional<std::filesystem::path>   m_filePath;
    NodeGraph::NodeGraphEditor             m_canvas;
    std::vector<Dialog::ValidationIssue>   m_issues;
    bool                                   m_issuesPopupOpen = false;
};

} // namespace Mood
