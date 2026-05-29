// F4H1: Inspector — HealthComponent en la categoría Gameplay.
// Cubre: current/max sliders + checkbox dead (debug) + reset a default
// de Project Settings.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

namespace Mood {

void InspectorPanel::renderHealthSection(Entity e) {
    auto& h = e.getComponent<HealthComponent>();
    if (!beginComponentSection<HealthComponent>(e, ICON_FA_GAMEPAD " Salud")) return;

    // F4H1: reset usa el default canónico (100). El dev puede ajustar el
    // default global via Project Settings > Gameplay > Salud máxima por
    // defecto, pero la conexión live al Inspector queda agendizada
    // (require pasar ProjectSettings via EditorUI getter — F4H1.5+).
    constexpr f32 maxDefault = 100.0f;

    // current — slider 0..max.
    const std::string currentLabel = I18n::T("editor.panel.inspector.health.current") + "##hc";
    if (ImGui::SliderFloat(currentLabel.c_str(), &h.current, 0.0f, h.max, "%.0f HP")) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, h.current,
        [](Entity& en, const f32& v) {
            en.getComponent<HealthComponent>().current = v;
        },
        "Editar salud current");

    // max — slider 1..1000.
    const std::string maxLabel = I18n::T("editor.panel.inspector.health.max") + "##hm";
    if (ImGui::SliderFloat(maxLabel.c_str(), &h.max, 1.0f, 1000.0f, "%.0f HP")) {
        if (h.current > h.max) h.current = h.max;  // clamp current al max
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, h.max,
        [](Entity& en, const f32& v) {
            en.getComponent<HealthComponent>().max = v;
        },
        "Editar salud max");
    if (detail::inspectorResetButton<f32>(m_ui, e, "hc_max",
            h.max, maxDefault,
            [](Entity& en, const f32& v) {
                if (en.hasComponent<HealthComponent>())
                    en.getComponent<HealthComponent>().max = v;
            },
            "Reset salud max")) {
        m_editedThisFrame = true;
    }

    // dead — checkbox debug. Útil para forzar el estado dead en testing.
    const std::string deadLabel = I18n::T("editor.panel.inspector.health.dead") + "##hd";
    if (ImGui::Checkbox(deadLabel.c_str(), &h.dead)) {
        m_editedThisFrame = true;
        if (h.dead && h.current > 0.0f) h.current = 0.0f;
    }
    detail::pushEditIfDone<bool>(m_editTracker, m_ui, e, h.dead,
        [](Entity& en, const bool& v) {
            en.getComponent<HealthComponent>().dead = v;
        },
        "Toggle salud dead");
}

} // namespace Mood
