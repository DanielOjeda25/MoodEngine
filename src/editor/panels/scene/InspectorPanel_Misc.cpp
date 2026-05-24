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

    const std::string playerLabel = I18n::T("editor.panel.inspector.trigger.triggers_on_player") + "##trig";
    if (ImGui::Checkbox(playerLabel.c_str(), &tc.triggersOnPlayer)) m_editedThisFrame = true;
    const std::string oneShotLabel = I18n::T("editor.panel.inspector.trigger.one_shot") + "##trig";
    if (ImGui::Checkbox(oneShotLabel.c_str(), &tc.oneShot)) m_editedThisFrame = true;
    const std::string enabledLabel = I18n::T("editor.panel.inspector.trigger.enabled") + "##trig";
    if (ImGui::Checkbox(enabledLabel.c_str(), &tc.enabled)) m_editedThisFrame = true;

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
    const char* shapeNames[] = {"Box", "Sphere"};
    int shapeIdx = static_cast<int>(ff.shape);
    const std::string shapeLabel = I18n::T("editor.panel.inspector.force_field.shape") + "##ff";
    if (ImGui::Combo(shapeLabel.c_str(), &shapeIdx, shapeNames, 2)) {
        ff.shape = static_cast<ForceFieldComponent::Shape>(shapeIdx);
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
    const char* modeNames[] = {"Directional", "Radial"};
    int modeIdx = static_cast<int>(ff.mode);
    const std::string modeLabel = I18n::T("editor.panel.inspector.force_field.mode") + "##ff";
    if (ImGui::Combo(modeLabel.c_str(), &modeIdx, modeNames, 2)) {
        ff.mode = static_cast<ForceFieldComponent::Mode>(modeIdx);
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
        const std::string falloffLabel = I18n::T("editor.panel.inspector.force_field.linear_falloff") + "##ff";
        if (ImGui::Checkbox(falloffLabel.c_str(), &ff.linearFalloff)) {
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
    const std::string imLabel = I18n::T("editor.panel.inspector.force_field.ignore_mass") + "##ff";
    if (ImGui::Checkbox(imLabel.c_str(), &ff.ignoreMass)) m_editedThisFrame = true;
    detail::helpMarker(I18n::T("editor.panel.inspector.force_field.ignore_mass_help").c_str());
    const std::string enLabel = I18n::T("editor.panel.inspector.force_field.enabled") + "##ff";
    if (ImGui::Checkbox(enLabel.c_str(), &ff.enabled)) m_editedThisFrame = true;

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
    int rx = cl.resX;
    const std::string rxLabel =
        I18n::T("editor.panel.inspector.cloth.res_x") + "##cloth";
    if (ImGui::SliderInt(rxLabel.c_str(), &rx, 2, 40)) {
        cl.resX = rx; m_editedThisFrame = true; simChanged = true;
    }
    int ry = cl.resY;
    const std::string ryLabel =
        I18n::T("editor.panel.inspector.cloth.res_y") + "##cloth";
    if (ImGui::SliderInt(ryLabel.c_str(), &ry, 2, 40)) {
        cl.resY = ry; m_editedThisFrame = true; simChanged = true;
    }

    // --- Anclaje ---
    const char* anchorNames[] = {"None", "Top Edge", "Top Corners", "Left Edge"};
    int aIdx = static_cast<int>(cl.anchor);
    const std::string aLabel =
        I18n::T("editor.panel.inspector.cloth.anchor") + "##cloth";
    if (ImGui::Combo(aLabel.c_str(), &aIdx, anchorNames, 4)) {
        cl.anchor = static_cast<ClothComponent::Anchor>(aIdx);
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

    const std::string gLabel =
        I18n::T("editor.panel.inspector.cloth.use_gravity") + "##cloth";
    if (ImGui::Checkbox(gLabel.c_str(), &cl.useGravity)) {
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
