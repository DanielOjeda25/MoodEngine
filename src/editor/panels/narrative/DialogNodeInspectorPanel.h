#pragma once

// DialogNodeInspectorPanel (F2H47): inspector contextual del nodo
// seleccionado en el DialogEditorPanel. Muestra los campos del
// `customData` (text_key/text_literal/portrait/audio/animation +
// choices array con add/remove buttons) editables inline.
//
// Auto-sync invariant (F2H47 schema v1): cambiar `choices` array
// dispara `asset.writeLine(node, line)` que sincroniza los output
// sockets del nodo en el grafo subyacente.

#include "editor/panels/IPanel.h"

namespace Mood {

class EditorUI;

class DialogNodeInspectorPanel : public IPanel {
public:
    DialogNodeInspectorPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Dialog Node Inspector"; }
    const char* category() const override { return "Narrative"; }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    EditorUI* m_ui = nullptr;
};

} // namespace Mood
