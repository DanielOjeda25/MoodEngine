#pragma once

// Toolbar lateral del editor (F2H22). Panel ImGui flotante que el dev
// puede acoplar al borde izquierdo del Viewport. Contiene botones para
// las acciones de edicion mas frecuentes:
//
//   - Gizmo modes: Translate (T), Rotate (R), Scale (S).
//   - Brushes: Add Box, Add Cylinder.
//   - Sub-mode: Face mode toggle.
//
// Las acciones emiten `request*` al EditorUI, que EditorApplication
// consume cada frame en `run()`. Sin acoplamiento directo con
// EditorApplication.
//
// Iconos = caracteres ASCII grandes (T / R / S / Box / Cyl / Face)
// para que sea legible sin font icons. El upgrade a iconos image-based
// queda como pendiente futuro.

#include "editor/panels/IPanel.h"

namespace Mood {

class EditorUI;

class Toolbar : public IPanel {
public:
    Toolbar() {
        // Visible por default — el dev puede ocultarlo desde menu Ver.
        visible = true;
    }

    void onImGuiRender() override;
    const char* name() const override { return "Tools"; }
    const char* category() const override { return "Scene"; }

    void setEditorUi(EditorUI* ui) { m_ui = ui; }

private:
    EditorUI* m_ui = nullptr;
};

} // namespace Mood
