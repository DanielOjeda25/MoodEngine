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

private:
    // Filtro por substring dentro del campo `channel` de cada entry.
    // "" = sin filtro.
    std::array<char, 32> m_channelFilter{};
    bool m_autoScroll = true;
};

} // namespace Mood
