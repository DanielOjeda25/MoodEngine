// F2H24: Inspector — secciones simples (Tag, Camera, Trigger).
// Componentes con UI corta que no justifican su propio archivo.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"  // F2H43
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <cstdio>
#include <string>

namespace Mood {

void InspectorPanel::renderTagSection(Entity e) {
    auto& tag = e.getComponent<TagComponent>();
    ImGui::SeparatorText(ICON_FA_TAG " Tag");
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s", tag.name.c_str());
    if (ImGui::InputText("##tag", buf, sizeof(buf))) {
        tag.name = buf;
        m_editedThisFrame = true;
    }
    // Hito 32 D: undo/redo del nombre. InputText reporta IsItemDeactivatedAfterEdit
    // cuando el dev sale del campo (Tab, click fuera, Enter).
    detail::pushEditIfDone<std::string>(m_editTracker, m_ui, e, tag.name,
        [](Entity& en, const std::string& v) {
            en.getComponent<TagComponent>().name = v;
        },
        "Renombrar entidad");
    ImGui::Separator();
}

void InspectorPanel::renderCameraSection(Entity e) {
    auto& cam = e.getComponent<CameraComponent>();
    if (!beginComponentSection<CameraComponent>(e, ICON_FA_VIDEO " Camera")) return;
    // break-A7 (auditoria): CameraComponent es stub. Los 3 campos
    // (fovDeg/nearPlane/farPlane) se editan, se serializan via
    // EditPropertyCommand y se "guardan" en el componente, pero el render
    // sigue usando `m_editorCamera.fovDeg()` — el componente no esta
    // cableado al pipeline. Para no engañar al dev hacemos los 3 widgets
    // read-only (BeginDisabled) y agregamos un label "(stub)". El sistema
    // de "active camera" que derive proyeccion del CameraComponent activo
    // es un item de Fase 3.
    ImGui::TextDisabled("(stub: no afecta al render)");
    ImGui::BeginDisabled(true);
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.camera.fov", "##cam", cam.fovDeg,
            [](Entity& en, const f32& v) {
                en.getComponent<CameraComponent>().fovDeg = v;
            },
            "Editar camera fov", 0.1f, 1.0f, 179.0f)) {
        m_editedThisFrame = true;
    }
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.camera.near", "##cam", cam.nearPlane,
            [](Entity& en, const f32& v) {
                en.getComponent<CameraComponent>().nearPlane = v;
            },
            "Editar camera near", 0.001f, 0.001f, 100.0f)) {
        m_editedThisFrame = true;
    }
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.camera.far", "##cam", cam.farPlane,
            [](Entity& en, const f32& v) {
                en.getComponent<CameraComponent>().farPlane = v;
            },
            "Editar camera far", 0.1f, 1.0f, 10000.0f)) {
        m_editedThisFrame = true;
    }
    ImGui::EndDisabled();
    ImGui::Separator();
}

void InspectorPanel::renderTriggerSection(Entity e) {
    auto& tc = e.getComponent<TriggerComponent>();
    if (!beginComponentSection<TriggerComponent>(e, ICON_FA_BORDER_NONE " Trigger")) return;
    if (detail::fieldDragFloat3(m_editTracker, m_ui, e,
            "editor.panel.inspector.trigger.half_extents", "##trig", tc.halfExtents,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<TriggerComponent>().halfExtents = v;
            },
            "Editar trigger halfExtents", 0.05f, 0.01f, 100.0f)) {
        m_editedThisFrame = true;
    }

    // --- F2H73: filtros + flags avanzados ---
    char tagBuf[128];
    std::snprintf(tagBuf, sizeof(tagBuf), "%s", tc.requiredTag.c_str());
    const std::string tagLabel = I18n::T("editor.panel.inspector.trigger.required_tag") + "##trig";
    if (ImGui::InputText(tagLabel.c_str(), tagBuf, sizeof(tagBuf))) {
        tc.requiredTag = tagBuf;
        m_editedThisFrame = true;
    }
    // F2H74: pushEditIfDone ANTES del helpMarker (ver nota en Joint) — si no,
    // el tracker lee el ID del "(?)" y el undo nunca dispara.
    detail::pushEditIfDone<std::string>(m_editTracker, m_ui, e, tc.requiredTag,
        [](Entity& en, const std::string& v) {
            en.getComponent<TriggerComponent>().requiredTag = v;
        },
        "Editar trigger requiredTag");
    detail::helpMarker(I18n::T("editor.panel.inspector.trigger.required_tag_help").c_str());

    // F3H12: los 3 checkboxes con undo + multi-edit.
    const bool activeTrigPlayer = tc.triggersOnPlayer;
    if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.trigger.triggers_on_player", "##trig", tc.triggersOnPlayer,
            [activeTrigPlayer](Entity en) -> bool {
                if (!en.hasComponent<TriggerComponent>()) return activeTrigPlayer;
                return en.getComponent<TriggerComponent>().triggersOnPlayer;
            },
            [](Entity& en, const bool& v) {
                if (!en.hasComponent<TriggerComponent>()) return;
                en.getComponent<TriggerComponent>().triggersOnPlayer = v;
            },
            "Toggle trigger triggersOnPlayer")) {
        m_editedThisFrame = true;
    }
    const bool activeTrigOneShot = tc.oneShot;
    if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.trigger.one_shot", "##trig", tc.oneShot,
            [activeTrigOneShot](Entity en) -> bool {
                if (!en.hasComponent<TriggerComponent>()) return activeTrigOneShot;
                return en.getComponent<TriggerComponent>().oneShot;
            },
            [](Entity& en, const bool& v) {
                if (!en.hasComponent<TriggerComponent>()) return;
                en.getComponent<TriggerComponent>().oneShot = v;
            },
            "Toggle trigger oneShot")) {
        m_editedThisFrame = true;
    }
    const bool activeTrigEnabled = tc.enabled;
    if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.trigger.enabled", "##trig", tc.enabled,
            [activeTrigEnabled](Entity en) -> bool {
                if (!en.hasComponent<TriggerComponent>()) return activeTrigEnabled;
                return en.getComponent<TriggerComponent>().enabled;
            },
            [](Entity& en, const bool& v) {
                if (!en.hasComponent<TriggerComponent>()) return;
                en.getComponent<TriggerComponent>().enabled = v;
            },
            "Toggle trigger enabled")) {
        m_editedThisFrame = true;
    }

    ImGui::TextDisabled("%s",
        I18n::T(tc.playerInside ? "editor.panel.inspector.trigger.player_inside_yes"
                                  : "editor.panel.inspector.trigger.player_inside_no").c_str());
    if (tc.oneShot && tc.fired) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.trigger.fired").c_str());
    }
    ImGui::Separator();
}

void InspectorPanel::renderForceFieldSection(Entity e) {
    auto& ff = e.getComponent<ForceFieldComponent>();
    if (!beginComponentSection<ForceFieldComponent>(e, ICON_FA_MAGNET " Force Field")) return;

    // --- Shape combo + parametro de la zona ---
    // F3H12: shape combo con undo + multi-edit.
    static const char* shapeNames[] = {"Box", "Sphere"};
    u32 shapeU32 = static_cast<u32>(ff.shape);
    const u32 activeShape = shapeU32;
    if (detail::multiEditCombo(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.force_field.shape", "##ff", shapeU32,
            shapeNames, 2,
            [activeShape](Entity en) -> u32 {
                if (!en.hasComponent<ForceFieldComponent>()) return activeShape;
                return static_cast<u32>(en.getComponent<ForceFieldComponent>().shape);
            },
            [](Entity& en, const u32& v) {
                if (!en.hasComponent<ForceFieldComponent>()) return;
                en.getComponent<ForceFieldComponent>().shape =
                    static_cast<ForceFieldComponent::Shape>(v);
            },
            "Cambiar force field shape")) {
        ff.shape = static_cast<ForceFieldComponent::Shape>(shapeU32);
        m_editedThisFrame = true;
    }
    if (ff.shape == ForceFieldComponent::Shape::Box) {
        if (detail::fieldDragFloat3(m_editTracker, m_ui, e,
                "editor.panel.inspector.force_field.half_extents", "##ff", ff.halfExtents,
                [](Entity& en, const glm::vec3& v) {
                    en.getComponent<ForceFieldComponent>().halfExtents = v;
                },
                "Editar force field halfExtents", 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
    } else {
        if (detail::fieldDragFloat(m_editTracker, m_ui, e,
                "editor.panel.inspector.force_field.radius", "##ff", ff.radius,
                [](Entity& en, const f32& v) {
                    en.getComponent<ForceFieldComponent>().radius = v;
                },
                "Editar force field radius", 0.05f, 0.01f, 100.0f)) {
            m_editedThisFrame = true;
        }
    }

    // --- Mode combo + parametro especifico ---
    // F3H12: mode combo con undo + multi-edit.
    static const char* modeNames[] = {"Directional", "Radial"};
    u32 modeU32 = static_cast<u32>(ff.mode);
    const u32 activeMode = modeU32;
    if (detail::multiEditCombo(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.force_field.mode", "##ff", modeU32,
            modeNames, 2,
            [activeMode](Entity en) -> u32 {
                if (!en.hasComponent<ForceFieldComponent>()) return activeMode;
                return static_cast<u32>(en.getComponent<ForceFieldComponent>().mode);
            },
            [](Entity& en, const u32& v) {
                if (!en.hasComponent<ForceFieldComponent>()) return;
                en.getComponent<ForceFieldComponent>().mode =
                    static_cast<ForceFieldComponent::Mode>(v);
            },
            "Cambiar force field mode")) {
        ff.mode = static_cast<ForceFieldComponent::Mode>(modeU32);
        m_editedThisFrame = true;
    }
    if (ff.mode == ForceFieldComponent::Mode::Directional) {
        if (detail::fieldDragFloat3(m_editTracker, m_ui, e,
                "editor.panel.inspector.force_field.direction", "##ff", ff.direction,
                [](Entity& en, const glm::vec3& v) {
                    en.getComponent<ForceFieldComponent>().direction = v;
                },
                "Editar force field direction", 0.01f, -1.0f, 1.0f)) {
            m_editedThisFrame = true;
        }
    } else {
        // F3H12: linearFalloff con undo + multi-edit.
        const bool activeLinear = ff.linearFalloff;
        if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
                "editor.panel.inspector.force_field.linear_falloff", "##ff",
                ff.linearFalloff,
                [activeLinear](Entity en) -> bool {
                    if (!en.hasComponent<ForceFieldComponent>()) return activeLinear;
                    return en.getComponent<ForceFieldComponent>().linearFalloff;
                },
                [](Entity& en, const bool& v) {
                    if (!en.hasComponent<ForceFieldComponent>()) return;
                    en.getComponent<ForceFieldComponent>().linearFalloff = v;
                },
                "Toggle force field linearFalloff")) {
            m_editedThisFrame = true;
        }
    }

    // --- Strength (compartido) ---
    // F2H74: el helper deja pushEditIfDone justo tras el widget y el
    // helpMarker despues — sin esto el undo quedaba muerto por el "(?)".
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.force_field.strength", "##ff", ff.strength,
            [](Entity& en, const f32& v) {
                en.getComponent<ForceFieldComponent>().strength = v;
            },
            "Editar force field strength", 0.5f, -10000.0f, 10000.0f)) {
        m_editedThisFrame = true;
    }
    detail::helpMarker(I18n::T("editor.panel.inspector.force_field.strength_help").c_str());

    // --- Toggles ---
    // F3H12: ignoreMass + enabled checkboxes con undo + multi-edit.
    const bool activeIgnoreMass = ff.ignoreMass;
    if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.force_field.ignore_mass", "##ff", ff.ignoreMass,
            [activeIgnoreMass](Entity en) -> bool {
                if (!en.hasComponent<ForceFieldComponent>()) return activeIgnoreMass;
                return en.getComponent<ForceFieldComponent>().ignoreMass;
            },
            [](Entity& en, const bool& v) {
                if (!en.hasComponent<ForceFieldComponent>()) return;
                en.getComponent<ForceFieldComponent>().ignoreMass = v;
            },
            "Toggle force field ignoreMass")) {
        m_editedThisFrame = true;
    }
    detail::helpMarker(I18n::T("editor.panel.inspector.force_field.ignore_mass_help").c_str());
    const bool activeFfEnabled = ff.enabled;
    if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.force_field.enabled", "##ff", ff.enabled,
            [activeFfEnabled](Entity en) -> bool {
                if (!en.hasComponent<ForceFieldComponent>()) return activeFfEnabled;
                return en.getComponent<ForceFieldComponent>().enabled;
            },
            [](Entity& en, const bool& v) {
                if (!en.hasComponent<ForceFieldComponent>()) return;
                en.getComponent<ForceFieldComponent>().enabled = v;
            },
            "Toggle force field enabled")) {
        m_editedThisFrame = true;
    }

    ImGui::Separator();
}

// F2H75: Cloth. Editar los params geometricos/sim marca `dirty` para que el
// ClothSystem rematerialice el soft body; el color es live (lo lee el
// renderer cada frame, sin recrear la tela).
void InspectorPanel::renderClothSection(Entity e) {
    auto& cl = e.getComponent<ClothComponent>();
    if (!beginComponentSection<ClothComponent>(
            e, I18n::T("component.name.cloth").c_str())) return;

    bool simChanged = false;  // requiere re-materializar

    // --- Dimensiones ---
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.width", "##cloth", cl.width,
            [](Entity& en, const f32& v) {
                en.getComponent<ClothComponent>().width = v;
            },
            "Editar cloth width", 0.05f, 0.05f, 50.0f)) {
        m_editedThisFrame = true; simChanged = true;
    }
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.height", "##cloth", cl.height,
            [](Entity& en, const f32& v) {
                en.getComponent<ClothComponent>().height = v;
            },
            "Editar cloth height", 0.05f, 0.05f, 50.0f)) {
        m_editedThisFrame = true; simChanged = true;
    }

    // --- Resolucion (particulas por lado) ---
    // F3H12: resX/resY SliderInt con undo via pushEditIfDone<u32>.
    // El tracker lee el ID del ultimo widget (SliderInt) y captura
    // el u32 actual (post-edit). Setter castea de vuelta a int +
    // dirty=true para que el ClothSystem rematerialize.
    int rx = cl.resX;
    const std::string rxLabel =
        I18n::T("editor.panel.inspector.cloth.res_x") + "##cloth";
    if (ImGui::SliderInt(rxLabel.c_str(), &rx, 2, 40)) {
        cl.resX = rx; m_editedThisFrame = true; simChanged = true;
    }
    detail::pushEditIfDone<u32>(m_editTracker, m_ui, e,
        static_cast<u32>(cl.resX),
        [](Entity& en, const u32& v) {
            auto& c = en.getComponent<ClothComponent>();
            c.resX = static_cast<int>(v);
            c.dirty = true;
        },
        "Editar cloth resX");
    int ry = cl.resY;
    const std::string ryLabel =
        I18n::T("editor.panel.inspector.cloth.res_y") + "##cloth";
    if (ImGui::SliderInt(ryLabel.c_str(), &ry, 2, 40)) {
        cl.resY = ry; m_editedThisFrame = true; simChanged = true;
    }
    detail::pushEditIfDone<u32>(m_editTracker, m_ui, e,
        static_cast<u32>(cl.resY),
        [](Entity& en, const u32& v) {
            auto& c = en.getComponent<ClothComponent>();
            c.resY = static_cast<int>(v);
            c.dirty = true;
        },
        "Editar cloth resY");

    // --- Anclaje ---
    // F3H12: anchor combo con undo + multi-edit. Setter incluye dirty=true.
    static const char* anchorNames[] = {"None", "Top Edge", "Top Corners", "Left Edge"};
    u32 anchorU32 = static_cast<u32>(cl.anchor);
    const u32 activeAnchor = anchorU32;
    if (detail::multiEditCombo(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.anchor", "##cloth", anchorU32,
            anchorNames, 4,
            [activeAnchor](Entity en) -> u32 {
                if (!en.hasComponent<ClothComponent>()) return activeAnchor;
                return static_cast<u32>(en.getComponent<ClothComponent>().anchor);
            },
            [](Entity& en, const u32& v) {
                if (!en.hasComponent<ClothComponent>()) return;
                auto& c = en.getComponent<ClothComponent>();
                c.anchor = static_cast<ClothComponent::Anchor>(v);
                c.dirty = true;
            },
            "Cambiar cloth anchor")) {
        cl.anchor = static_cast<ClothComponent::Anchor>(anchorU32);
        m_editedThisFrame = true; simChanged = true;
    }

    // --- Parametros de simulacion ---
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.mass", "##cloth", cl.totalMass,
            [](Entity& en, const f32& v) {
                en.getComponent<ClothComponent>().totalMass = v;
            },
            "Editar cloth mass", 0.05f, 0.01f, 100.0f)) {
        m_editedThisFrame = true; simChanged = true;
    }
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.stiffness", "##cloth", cl.stiffness,
            [](Entity& en, const f32& v) {
                en.getComponent<ClothComponent>().stiffness = v;
            },
            "Editar cloth stiffness", 0.01f, 0.0f, 1.0f)) {
        m_editedThisFrame = true; simChanged = true;
    }
    if (detail::fieldDragFloat(m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.damping", "##cloth", cl.damping,
            [](Entity& en, const f32& v) {
                en.getComponent<ClothComponent>().damping = v;
            },
            "Editar cloth damping", 0.01f, 0.0f, 2.0f)) {
        m_editedThisFrame = true; simChanged = true;
    }

    // F3H12: useGravity con undo + multi-edit. Setter incluye dirty=true.
    const bool activeUseGravity = cl.useGravity;
    if (detail::multiEditCheckbox(m_multiEditTracker, m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.use_gravity", "##cloth", cl.useGravity,
            [activeUseGravity](Entity en) -> bool {
                if (!en.hasComponent<ClothComponent>()) return activeUseGravity;
                return en.getComponent<ClothComponent>().useGravity;
            },
            [](Entity& en, const bool& v) {
                if (!en.hasComponent<ClothComponent>()) return;
                auto& c = en.getComponent<ClothComponent>();
                c.useGravity = v;
                c.dirty = true;
            },
            "Toggle cloth useGravity")) {
        m_editedThisFrame = true; simChanged = true;
    }

    // --- Color (live, no re-materializa) ---
    if (detail::fieldColorEdit3(m_editTracker, m_ui, e,
            "editor.panel.inspector.cloth.color", "##cloth", cl.color,
            [](Entity& en, const glm::vec3& v) {
                en.getComponent<ClothComponent>().color = v;
            },
            "Editar cloth color")) {
        m_editedThisFrame = true;
    }

    if (simChanged) cl.dirty = true;
    ImGui::Separator();
}

} // namespace Mood
