#pragma once

// Panel que muestra los últimos logs capturados en memoria por el
// `LogRingSink` (Hito 5 Bloque 6). Color por nivel, filtro por canal
// (input de texto) y auto-scroll opcional.

#include "editor/panels/IPanel.h"

#include <array>

namespace Mood {

class ConsolePanel : public IPanel {
public:
    void onImGuiRender() override;
    const char* name() const override { return "Console"; }
    const char* category() const override { return "Debug"; }

private:
    // F2H37: filtro por nivel — 6 toggles independientes (trace, debug,
    // info, warn, err, critical). Default = todos visibles. Si un nivel
    // esta off, sus entries se skipean al iterar el snapshot del sink.
    std::array<bool, 6> m_levelEnabled{ true, true, true, true, true, true };

    /// F3H24: filtro por mensaje. Match case-insensitive contra
    /// `entry.text + " " + entry.channel`. Reemplaza el viejo filtro
    /// por channel (mucho mas util en day-to-day — el dev tipea
    /// "vehicle" / "shader" / un keyword del bug y filtra todo el log).
    std::array<char, 64> m_messageFilter{};
    bool m_autoScroll = true;
};

} // namespace Mood
