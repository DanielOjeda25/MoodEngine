#pragma once

// F3H23: panel del profiler in-engine. Lee del `ProfilerBuffer` global
// (alimentado por MOOD_PROFILE_SCOPE) y muestra:
//   - Tabla por scope: Avg | Min | Max | Last | Hits/frame | %frame
//   - Histograma de los últimos N frames (total ms instrumentado).
//   - Selector de "ventana" (últimos M frames) — clampeado al capacity.
//
// Off por defecto; toggle desde View > Debug. Independiente del
// PerformanceHudPanel (que tiene FPS/draws/tris a alto nivel).

#include "editor/panels/IPanel.h"

namespace Mood {

class ProfilerPanel : public IPanel {
public:
    ProfilerPanel() { visible = false; }

    void onImGuiRender() override;
    const char* name() const override { return "Profiler"; }
    const char* category() const override { return "Debug"; }

private:
    /// Ventana de agregación: cuántos frames del ring se promediarán
    /// (0 = todos los disponibles). Sticky entre frames; clampeado al
    /// capacity del ring antes de cada cálculo.
    int m_aggregateWindow = 60;
    /// Cuando true, el histograma renderiza. Toggleable porque puede ser
    /// caro con N grande (1200 samples).
    bool m_showHistogram = true;
};

} // namespace Mood
