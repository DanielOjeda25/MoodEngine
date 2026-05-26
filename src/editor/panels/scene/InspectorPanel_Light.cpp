// F2H24: Inspector — LightComponent. EnvironmentComponent vive en
// InspectorPanel_Environment.cpp desde F2H58 Bloque A (pre-F2H58 estaba
// aca por accidente historico).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <memory>

namespace Mood {

// F3H13: defaults del LightComponent — fuente unica para los reset
// buttons. Reflejan la construccion `LightComponent{}` (en sync con
// Components_Render.h:209-215).
namespace {
constexpr LightComponent::Type kLightDefaultType = LightComponent::Type::Point;
constexpr float kLightDefaultIntensity = 1.0f;
constexpr float kLightDefaultRadius    = 10.0f;
constexpr bool  kLightDefaultEnabled     = true;
constexpr bool  kLightDefaultCastShadows = false;
const glm::vec3 kLightDefaultColor     {1.0f, 1.0f, 1.0f};
const glm::vec3 kLightDefaultDirection {0.0f, -1.0f, 0.0f};
} // namespace

// LightComponent (Hito 11)
// Activado: tiene type / color / intensity (Hito 7) + radius (point) +
// direction (directional) + enabled.
void InspectorPanel::renderLightSection(Entity e) {
    auto& lt = e.getComponent<LightComponent>();
    if (!beginComponentSection<LightComponent>(e, ICON_FA_LIGHTBULB " Light")) return;

    // F3H12: enabled checkbox con undo + multi-edit.
    const bool activeEnabled = lt.enabled;
    if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.light.enabled", "##lt", lt.enabled,
            [activeEnabled](Entity en) -> bool {
                if (!en.hasComponent<LightComponent>()) return activeEnabled;
                return en.getComponent<LightComponent>().enabled;
            },
            [](Entity& en, const bool& v) {
                if (!en.hasComponent<LightComponent>()) return;
                en.getComponent<LightComponent>().enabled = v;
            },
            "Toggle light enabled")) {
        m_editedThisFrame = true;
    }
    if (detail::inspectorResetButton<bool>(m_ui, e, "lt_enabled",
            lt.enabled, kLightDefaultEnabled,
            [](Entity& en, const bool& v) {
                if (en.hasComponent<LightComponent>())
                    en.getComponent<LightComponent>().enabled = v;
            },
            "Reset Light enabled")) {
        m_editedThisFrame = true;
    }

    const char* items[] = {"Directional", "Point"};
    int current = static_cast<int>(lt.type);
    const std::string typeLabel = I18n::T("editor.panel.inspector.light.type") + "##lt";
    if (ImGui::Combo(typeLabel.c_str(), &current, items, 2)) {
        // Hito 40 F: undo del light type combo.
        const auto oldType = lt.type;
        const auto newType = static_cast<LightComponent::Type>(current);
        if (oldType != newType) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, static_cast<u32>(oldType), static_cast<u32>(newType),
                    [](Entity& en, const u32& v) {
                        en.getComponent<LightComponent>().type =
                            static_cast<LightComponent::Type>(v);
                    },
                    "Cambiar Light type");
                h->push(std::move(cmd));
            } else {
                lt.type = newType;
            }
            m_editedThisFrame = true;
        }
    }
    if (detail::inspectorResetButton<u32>(m_ui, e, "lt_type",
            static_cast<u32>(lt.type), static_cast<u32>(kLightDefaultType),
            [](Entity& en, const u32& v) {
                if (en.hasComponent<LightComponent>())
                    en.getComponent<LightComponent>().type =
                        static_cast<LightComponent::Type>(v);
            },
            "Reset Light type")) {
        m_editedThisFrame = true;
    }
    // F3H8: multi-edit awareness. Si hay multi-seleccion con N luces,
    // el helper muestra "—" si los valores difieren, hace live preview
    // en peers, y pushea MultiEditPropertyCommand al soltar. Si hay 1
    // sola entidad, cae a fieldColorEdit3/fieldDragFloat (back-compat).
    // Los lambdas guardan contra entidades sin LightComponent (selecciones
    // mixtas tipo light+box): getter devuelve active value (mantiene
    // allMatch happy), setter no-opea.
    const glm::vec3 activeColor = lt.color;
    const f32 activeIntensity = lt.intensity;
    if (detail::multiEditColor3(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.light.color", "##lt", lt.color,
            [activeColor](Entity en) -> glm::vec3 {
                if (!en.hasComponent<LightComponent>()) return activeColor;
                return en.getComponent<LightComponent>().color;
            },
            [](Entity& en, const glm::vec3& v) {
                if (!en.hasComponent<LightComponent>()) return;
                en.getComponent<LightComponent>().color = v;
            },
            "Editar light color")) {
        m_editedThisFrame = true;
    }
    if (detail::inspectorResetButton<glm::vec3>(m_ui, e, "lt_color",
            lt.color, kLightDefaultColor,
            [](Entity& en, const glm::vec3& v) {
                if (en.hasComponent<LightComponent>())
                    en.getComponent<LightComponent>().color = v;
            },
            "Reset Light color")) {
        m_editedThisFrame = true;
    }
    if (detail::multiEditDragFloat(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.light.intensity", "##lt", lt.intensity,
            [activeIntensity](Entity en) -> f32 {
                if (!en.hasComponent<LightComponent>()) return activeIntensity;
                return en.getComponent<LightComponent>().intensity;
            },
            [](Entity& en, const f32& v) {
                if (!en.hasComponent<LightComponent>()) return;
                en.getComponent<LightComponent>().intensity = v;
            },
            "Editar light intensity", 0.01f, 0.0f, 100.0f)) {
        m_editedThisFrame = true;
    }
    if (detail::inspectorResetButton<f32>(m_ui, e, "lt_intensity",
            lt.intensity, kLightDefaultIntensity,
            [](Entity& en, const f32& v) {
                if (en.hasComponent<LightComponent>())
                    en.getComponent<LightComponent>().intensity = v;
            },
            "Reset Light intensity")) {
        m_editedThisFrame = true;
    }

    if (lt.type == LightComponent::Type::Point) {
        const f32 activeRadius = lt.radius;
        if (detail::multiEditDragFloat(m_multiEditTracker, m_editTracker, m_ui, e,
                "editor.panel.inspector.light.radius", "##lt", lt.radius,
                [activeRadius](Entity en) -> f32 {
                    if (!en.hasComponent<LightComponent>()) return activeRadius;
                    return en.getComponent<LightComponent>().radius;
                },
                [](Entity& en, const f32& v) {
                    if (!en.hasComponent<LightComponent>()) return;
                    en.getComponent<LightComponent>().radius = v;
                },
                "Editar light radius", 0.1f, 0.1f, 1000.0f)) {
            m_editedThisFrame = true;
        }
        if (detail::inspectorResetButton<f32>(m_ui, e, "lt_radius",
                lt.radius, kLightDefaultRadius,
                [](Entity& en, const f32& v) {
                    if (en.hasComponent<LightComponent>())
                        en.getComponent<LightComponent>().radius = v;
                },
                "Reset Light radius")) {
            m_editedThisFrame = true;
        }
    } else {
        // F3H12: direction DragFloat3 con undo (single-entity — la
        // direction es semanticamente diferente entre lights, no aplica
        // multi-edit "homogeneo" como color/intensity).
        if (detail::fieldDragFloat3(m_editTracker, m_ui, e,
                "editor.panel.inspector.light.direction", "##lt", lt.direction,
                [](Entity& en, const glm::vec3& v) {
                    if (!en.hasComponent<LightComponent>()) return;
                    en.getComponent<LightComponent>().direction = v;
                },
                "Editar light direction", 0.01f, -1.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
        if (detail::inspectorResetButton<glm::vec3>(m_ui, e, "lt_direction",
                lt.direction, kLightDefaultDirection,
                [](Entity& en, const glm::vec3& v) {
                    if (en.hasComponent<LightComponent>())
                        en.getComponent<LightComponent>().direction = v;
                },
                "Reset Light direction")) {
            m_editedThisFrame = true;
        }
        // Hito 16: solo directional puede emitir shadow map (point shadows
        // requeririan cubemap depth, fuera de scope).
        // F3H12: castShadows con undo + multi-edit.
        const bool activeCastShadows = lt.castShadows;
        if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
                "editor.panel.inspector.light.cast_shadows", "##lt", lt.castShadows,
                [activeCastShadows](Entity en) -> bool {
                    if (!en.hasComponent<LightComponent>()) return activeCastShadows;
                    return en.getComponent<LightComponent>().castShadows;
                },
                [](Entity& en, const bool& v) {
                    if (!en.hasComponent<LightComponent>()) return;
                    en.getComponent<LightComponent>().castShadows = v;
                },
                "Toggle light castShadows")) {
            m_editedThisFrame = true;
        }
        if (detail::inspectorResetButton<bool>(m_ui, e, "lt_castShadows",
                lt.castShadows, kLightDefaultCastShadows,
                [](Entity& en, const bool& v) {
                    if (en.hasComponent<LightComponent>())
                        en.getComponent<LightComponent>().castShadows = v;
                },
                "Reset Light castShadows")) {
            m_editedThisFrame = true;
        }
    }
    ImGui::Separator();
}

} // namespace Mood
