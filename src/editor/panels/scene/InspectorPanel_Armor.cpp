// F4H4: Inspector — ArmorComponent en la categoria Gameplay.
// Gemelo de Salud (InspectorPanel_Health.cpp).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

namespace Mood {

void InspectorPanel::renderArmorSection(Entity e) {
    auto& a = e.getComponent<ArmorComponent>();
    if (!beginComponentSection<ArmorComponent>(e, ICON_FA_GAMEPAD " Armadura")) return;

    constexpr f32 maxDefault         = 100.0f;
    constexpr f32 absorbRatioDefault = 0.66f;

    // current — slider 0..max.
    const std::string currentLabel =
        I18n::T("editor.panel.inspector.armor.current") + "##ac";
    if (ImGui::SliderFloat(currentLabel.c_str(), &a.current, 0.0f, a.max,
                            "%.0f AP")) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, a.current,
        [](Entity& en, const f32& v) {
            en.getComponent<ArmorComponent>().current = v;
        },
        "Editar armadura current");

    // max — slider 1..1000.
    const std::string maxLabel =
        I18n::T("editor.panel.inspector.armor.max") + "##am";
    if (ImGui::SliderFloat(maxLabel.c_str(), &a.max, 1.0f, 1000.0f, "%.0f AP")) {
        if (a.current > a.max) a.current = a.max;
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, a.max,
        [](Entity& en, const f32& v) {
            en.getComponent<ArmorComponent>().max = v;
        },
        "Editar armadura max");
    if (detail::inspectorResetButton<f32>(m_ui, e, "ac_max",
            a.max, maxDefault,
            [](Entity& en, const f32& v) {
                if (en.hasComponent<ArmorComponent>())
                    en.getComponent<ArmorComponent>().max = v;
            },
            "Reset armadura max")) {
        m_editedThisFrame = true;
    }

    // absorbRatio — slider 0..1. HL2 default 0.66.
    const std::string ratioLabel =
        I18n::T("editor.panel.inspector.armor.absorb_ratio") + "##ar";
    if (ImGui::SliderFloat(ratioLabel.c_str(), &a.absorbRatio, 0.0f, 1.0f,
                            "%.2f")) {
        m_editedThisFrame = true;
    }
    detail::pushEditIfDone<f32>(m_editTracker, m_ui, e, a.absorbRatio,
        [](Entity& en, const f32& v) {
            en.getComponent<ArmorComponent>().absorbRatio = v;
        },
        "Editar armadura absorbRatio");
    if (detail::inspectorResetButton<f32>(m_ui, e, "ac_ratio",
            a.absorbRatio, absorbRatioDefault,
            [](Entity& en, const f32& v) {
                if (en.hasComponent<ArmorComponent>())
                    en.getComponent<ArmorComponent>().absorbRatio = v;
            },
            "Reset armadura absorbRatio")) {
        m_editedThisFrame = true;
    }
}

} // namespace Mood
