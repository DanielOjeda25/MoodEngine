#pragma once

// NarrativeIntroPanel (F2H46): panel informativo del workspace
// "Narrativa". Explica al dev que esta sub-fase contiene infra del
// node-graph framework + roadmap de los editores reales (Dialog /
// Quest / Inventory) que vienen en proximos hitos.
//
// Existe porque el dev natural pregunta "como creo conversaciones?"
// y la respuesta v1 es "todavia no — esto es la base". Sin el panel,
// el workspace parece roto / inutilmente abstracto.
//
// El panel se reemplaza/oculta cuando los editores reales aterricen
// (F2H47+) — temporal pero util mientras tanto.

#include "editor/panels/IPanel.h"

namespace Mood {

class NarrativeIntroPanel : public IPanel {
public:
    NarrativeIntroPanel() { visible = false; }  // activa applyDefaultVisibility

    void onImGuiRender() override;
    const char* name() const override { return "Narrative Intro"; }
    const char* category() const override { return "Debug"; }
};

} // namespace Mood
