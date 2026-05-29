// F2H24: Inspector — RigidBodyComponent (type / shape / halfExtents /
// mass / friction).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "editor/ui/EditorUI.h"
#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <memory>

namespace Mood {

// RigidBodyComponent (Hito 12)
// Edit del shape/mass requiere recrear el body en Jolt, asi que por
// ahora los campos quedan read-only en Play Mode y editables en Editor.
// Al salir de Play, cualquier cambio toma efecto via rebuildSceneFromMap
// si se recarga el proyecto, o al destruir/recrear el body manualmente.
void InspectorPanel::renderRigidBodySection(Entity e) {
    auto& rb = e.getComponent<RigidBodyComponent>();
    if (!beginComponentSection<RigidBodyComponent>(e, ICON_FA_WEIGHT_HANGING " RigidBody")) return;
    // F3H13: defaults de RigidBodyComponent (en sync con el struct).
    constexpr u32 kRbDefaultType  = static_cast<u32>(RigidBodyComponent::Type::Dynamic);
    constexpr u32 kRbDefaultShape = static_cast<u32>(RigidBodyComponent::Shape::Box);
    static const glm::vec3 kRbDefaultHalfExtents{0.5f, 0.5f, 0.5f};
    constexpr f32  kRbDefaultMass     = 1.0f;
    constexpr f32  kRbDefaultFriction = 0.5f;
    constexpr bool kRbDefaultIsSensor = false;

    const char* typeNames[] = {"Static", "Kinematic", "Dynamic"};
    int typeIdx = static_cast<int>(rb.type);
    const std::string typeLabel = I18n::T("editor.panel.inspector.physics.type") + "##rb";
    if (ImGui::Combo(typeLabel.c_str(), &typeIdx, typeNames, 3)) {
        // Hito 40 F: undo via EditPropertyCommand<u32> (cambio
        // estructural, atomico — no usa el tracker drag-end).
        const auto oldType = rb.type;
        const auto newType = static_cast<RigidBodyComponent::Type>(typeIdx);
        if (oldType != newType) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, static_cast<u32>(oldType), static_cast<u32>(newType),
                    [](Entity& en, const u32& v) {
                        en.getComponent<RigidBodyComponent>().type =
                            static_cast<RigidBodyComponent::Type>(v);
                    },
                    "Cambiar RigidBody type");
                h->push(std::move(cmd));
            } else {
                rb.type = newType;
            }
            m_editedThisFrame = true;
        }
    }
    if (detail::inspectorResetButton<u32>(m_ui, e, "rb_type",
            static_cast<u32>(rb.type), kRbDefaultType,
            [](Entity& en, const u32& v) {
                if (en.hasComponent<RigidBodyComponent>())
                    en.getComponent<RigidBodyComponent>().type =
                        static_cast<RigidBodyComponent::Type>(v);
            },
            "Reset RigidBody type")) {
        m_editedThisFrame = true;
    }

    const char* shapeNames[] = {"Box", "Sphere", "Capsule"};
    int shapeIdx = static_cast<int>(rb.shape);
    const std::string shapeLabel = I18n::T("editor.panel.inspector.physics.shape") + "##rb";
    if (ImGui::Combo(shapeLabel.c_str(), &shapeIdx, shapeNames, 3)) {
        // Hito 40 F: undo del shape combo.
        const auto oldShape = rb.shape;
        const auto newShape = static_cast<RigidBodyComponent::Shape>(shapeIdx);
        if (oldShape != newShape) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<u32>>(
                    e, static_cast<u32>(oldShape), static_cast<u32>(newShape),
                    [](Entity& en, const u32& v) {
                        en.getComponent<RigidBodyComponent>().shape =
                            static_cast<RigidBodyComponent::Shape>(v);
                    },
                    "Cambiar RigidBody shape");
                h->push(std::move(cmd));
            } else {
                rb.shape = newShape;
            }
            m_editedThisFrame = true;
        }
    }

    if (detail::fieldDragFloat3(m_editTracker, m_ui, e,
            "editor.panel.inspector.physics.half_extents", "##rb", rb.halfExtents,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<RigidBodyComponent>().halfExtents = v;
            },
            "Editar rigid body halfExtents", 0.05f, 0.01f, 100.0f)) {
        m_editedThisFrame = true;
    }
    if (rb.type == RigidBodyComponent::Type::Dynamic) {
        if (detail::fieldDragFloat(m_editTracker, m_ui, e,
                "editor.panel.inspector.physics.mass", "##rb", rb.mass,
                [](Entity& en, const f32& v) {
                    en.getComponent<RigidBodyComponent>().mass = v;
                },
                "Editar rigid body mass", 0.1f, 0.001f, 10000.0f)) {
            m_editedThisFrame = true;
        }
        if (detail::inspectorResetButton<f32>(m_ui, e, "rb_mass",
                rb.mass, kRbDefaultMass,
                [](Entity& en, const f32& v) {
                    if (en.hasComponent<RigidBodyComponent>())
                        en.getComponent<RigidBodyComponent>().mass = v;
                },
                "Reset RigidBody mass")) {
            m_editedThisFrame = true;
        }
    }
    // Hito 34 A: friction. Aplica a static + dynamic (el contacto en
    // ambos lados afecta el comportamiento).
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.physics.friction", "##rb", rb.friction,
            [](Entity& en, const f32& v) {
                en.getComponent<RigidBodyComponent>().friction = v;
            },
            "Editar friction (RigidBody)", 0.01f, 0.0f, 2.0f)) {
        m_editedThisFrame = true;
    }
    if (detail::inspectorResetButton<f32>(m_ui, e, "rb_friction",
            rb.friction, kRbDefaultFriction,
            [](Entity& en, const f32& v) {
                if (en.hasComponent<RigidBodyComponent>())
                    en.getComponent<RigidBodyComponent>().friction = v;
            },
            "Reset RigidBody friction")) {
        m_editedThisFrame = true;
    }

    // break-A5 (2026-05-23): checkbox isSensor (trigger fisico estilo
    // Unity isTrigger). Llega a Jolt via mIsSensor desde F2H68. El
    // change rebuilea el body al proximo Play (mismo criterio que
    // type/shape combo); marcamos m_editedThisFrame para que el caller
    // sepa que la escena cambio.
    const std::string sensorLabel =
        I18n::T("editor.panel.inspector.physics.is_sensor") + "##rb";
    bool sensorBool = rb.isSensor;
    if (ImGui::Checkbox(sensorLabel.c_str(), &sensorBool)) {
        if (sensorBool != rb.isSensor) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<bool>>(
                    e, rb.isSensor, sensorBool,
                    [](Entity& en, const bool& v) {
                        en.getComponent<RigidBodyComponent>().isSensor = v;
                    },
                    "Cambiar RigidBody isSensor");
                h->push(std::move(cmd));
            } else {
                rb.isSensor = sensorBool;
            }
            m_editedThisFrame = true;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s",
            I18n::T("editor.panel.inspector.physics.is_sensor_tip").c_str());
    }
    if (detail::inspectorResetButton<bool>(m_ui, e, "rb_isSensor",
            rb.isSensor, kRbDefaultIsSensor,
            [](Entity& en, const bool& v) {
                if (en.hasComponent<RigidBodyComponent>())
                    en.getComponent<RigidBodyComponent>().isSensor = v;
            },
            "Reset RigidBody isSensor")) {
        m_editedThisFrame = true;
    }
    // F3H29 polish: hint `body_id_hint` eliminado (debug ID interno —
    // el dev casual no lo necesita, debug panel ya muestra physics state).
    ImGui::Separator();
}

// break-A4 (2026-05-23): Inspector de RagdollComponent. Pre-A4 los
// 4 campos (totalMass / limbRadius / useGravity / spawnImpulse) solo
// se editaban a mano en JSON. Convencion: range 1-300 kg para masa
// total (rango realista persona->bestia grande), 0.01-0.5 m para
// limbRadius (proxy capsule humano->gordo), impulse ilimitado (el
// dev decide la magnitud por contexto).
void InspectorPanel::renderRagdollSection(Entity e) {
    auto& rd = e.getComponent<RagdollComponent>();
    if (!beginComponentSection<RagdollComponent>(e, ICON_FA_PERSON_RUNNING " Ragdoll")) return;

    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.ragdoll.total_mass", "##rd", rd.totalMass,
            [](Entity& en, const f32& v) {
                en.getComponent<RagdollComponent>().totalMass = v;
            },
            "Editar ragdoll totalMass", 1.0f, 1.0f, 300.0f)) {
        m_editedThisFrame = true;
    }
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.ragdoll.limb_radius", "##rd", rd.limbRadius,
            [](Entity& en, const f32& v) {
                en.getComponent<RagdollComponent>().limbRadius = v;
            },
            "Editar ragdoll limbRadius", 0.005f, 0.01f, 0.5f)) {
        m_editedThisFrame = true;
    }

    const std::string gravLabel =
        I18n::T("editor.panel.inspector.ragdoll.use_gravity") + "##rd";
    bool gravBool = rd.useGravity;
    if (ImGui::Checkbox(gravLabel.c_str(), &gravBool)) {
        if (gravBool != rd.useGravity) {
            HistoryStack* h = m_ui ? m_ui->historyStack() : nullptr;
            if (h != nullptr) {
                auto cmd = std::make_unique<EditPropertyCommand<bool>>(
                    e, rd.useGravity, gravBool,
                    [](Entity& en, const bool& v) {
                        en.getComponent<RagdollComponent>().useGravity = v;
                    },
                    "Cambiar ragdoll useGravity");
                h->push(std::move(cmd));
            } else {
                rd.useGravity = gravBool;
            }
            m_editedThisFrame = true;
        }
    }

    if (detail::fieldDragFloat3(m_editTracker, m_ui, e,
            "editor.panel.inspector.ragdoll.spawn_impulse", "##rd", rd.spawnImpulse,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<RagdollComponent>().spawnImpulse = v;
            },
            "Editar ragdoll spawnImpulse", 0.5f, -100.0f, 100.0f)) {
        m_editedThisFrame = true;
    }
    // F3H29 polish: hint `state_hint` eliminado (debug interno: state +
    // ragdollId no aportan al dev casual).
    ImGui::Separator();
}

} // namespace Mood
