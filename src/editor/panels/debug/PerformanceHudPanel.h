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
    }

    void onImGuiRender() override;
    const char* name() const override { return "Performance"; }

    void setMetrics(const Metrics& m) { m_metrics = m; }

private:
    Metrics m_metrics{};
};

} // namespace Mood
