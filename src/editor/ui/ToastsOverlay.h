#pragma once

// F3H24: overlay del sistema de toasts (estilo VSCode bottom-right).
// Vive como helper standalone — NO es un IPanel porque:
//   - No tiene categoría / visibilidad toggleable (el overlay aparece
//     solo si hay toasts vivos).
//   - No se docka, no es movible.
//   - Se posiciona con `SetNextWindowPos` absoluto cada frame.
//
// El render es invocado desde `EditorUI::draw` después del Dockspace
// (los toasts están por encima de todo). El tick de aging vive en
// `EditorApplication::tickFrameMetrics` (gemelo del FpsCounter).

namespace Mood {

class ToastsOverlay {
public:
    /// @brief Renderiza los toasts vivos en la esquina inferior derecha.
    ///        Llamar una vez por frame desde EditorUI::draw después del
    ///        Dockspace. No-op si la cola está vacía o si
    ///        `UserSettings.editor.toastsEnabled` está en false.
    void draw();
};

} // namespace Mood
