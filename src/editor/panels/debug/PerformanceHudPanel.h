#pragma once

// Panel de metricas en vivo (F2H2). Muestra FPS, frame time, draw calls,
// triangulos del frame anterior, entity count del scene activo. Util para
// medir el coste de spawnear las escenas stress-test del menu Ayuda.
//
// Por defecto oculto: visible se togglea desde el menu "Ver > Performance".
// Las metricas las inyecta `EditorApplication` cada frame antes de
// `EditorUI::draw` via `setMetrics`.

#include "core/Types.h"
#include "editor/panels/IPanel.h"

namespace Mood {

class PerformanceHudPanel : public IPanel {
public:
    struct Metrics {
        f32 fps = 0.0f;
        f32 frameMs = 0.0f;
        u32 drawCalls = 0;
        u32 triangles = 0;
        u32 entityCount = 0;
    };

    PerformanceHudPanel() {
        visible = false;  // off por defecto; toggle desde menu Ver
        // Default label cubre el primer snapshot del benchmark F2H2 (escena
        // vacia). El dev lo cambia manualmente para 10K / 100K / 500K / 1M.
        m_snapshotLabel[0] = 'E'; m_snapshotLabel[1] = 'm'; m_snapshotLabel[2] = 'p';
        m_snapshotLabel[3] = 't'; m_snapshotLabel[4] = 'y'; m_snapshotLabel[5] = '\0';
    }

    void onImGuiRender() override;
    const char* name() const override { return "Performance"; }
    const char* category() const override { return "Debug"; }

    void setMetrics(const Metrics& m) { m_metrics = m; }

    /// @brief F2H42: setea el estado mostrado del checkbox VSync (sync con
    ///        el valor real del Window). El caller llama esto una vez al
    ///        inicio y luego cada vez que cambia VSync via API.
    void setVsyncState(bool enabled) { m_vsyncEnabled = enabled; }

    /// @brief F2H42: consume el toggle pedido por el dev en el checkbox.
    ///        Devuelve true + el nuevo valor pedido si hay cambio pendiente.
    ///        El caller aplica via `Window::setVSync` y luego sync con
    ///        `setVsyncState`.
    bool consumeVsyncToggleRequest(bool& outNewValue) {
        if (!m_vsyncToggleRequested) return false;
        m_vsyncToggleRequested = false;
        outNewValue = m_vsyncEnabled;
        return true;
    }

private:
    Metrics m_metrics{};
    // F2H2 Bloque G: snapshot dump → log + performance_baseline.csv
    char m_snapshotLabel[64]{};
    void writeSnapshot();

    // F2H42: estado del checkbox VSync. Cambios disparan
    // m_vsyncToggleRequested para que EditorApplication aplique en el Window.
    bool m_vsyncEnabled = true;
    bool m_vsyncToggleRequested = false;
};

} // namespace Mood
