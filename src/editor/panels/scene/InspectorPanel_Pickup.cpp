// F4H4: Inspector — PickupComponent en la categoria Gameplay.
// Dropdown del tipo + campos condicionales segun PickupType.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <string>

namespace Mood {

namespace {

const char* pickupTypeLabel(PickupType t) {
    switch (t) {
        case PickupType::Weapon: return "Weapon";
        case PickupType::Ammo:   return "Ammo";
        case PickupType::Health: return "Health";
        case PickupType::Armor:  return "Armor";
    }
    return "?";
}

} // anonymous

void InspectorPanel::renderPickupSection(Entity e) {
    auto& p = e.getComponent<PickupComponent>();
    if (!beginComponentSection<PickupComponent>(e, ICON_FA_GAMEPAD " Pickup")) return;

    // Type combo.
    const std::string typeLabel = I18n::T("editor.panel.inspector.pickup.type") + "##pt";
    if (ImGui::BeginCombo(typeLabel.c_str(), pickupTypeLabel(p.type))) {
        const PickupType types[] = { PickupType::Weapon, PickupType::Ammo,
                                       PickupType::Health, PickupType::Armor };
        for (PickupType t : types) {
            const bool selected = (p.type == t);
            if (ImGui::Selectable(pickupTypeLabel(t), selected)) {
                p.type = t;
                m_editedThisFrame = true;
            }
        }
        ImGui::EndCombo();
    }

    // Campos condicionales segun type.
    switch (p.type) {
        case PickupType::Weapon: {
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s", p.weaponPath.c_str());
            const std::string lbl = I18n::T("editor.panel.inspector.pickup.weapon_path") + "##ppw";
            if (ImGui::InputText(lbl.c_str(), buf, sizeof(buf))) {
                p.weaponPath = buf;
                m_editedThisFrame = true;
            }
            break;
        }
        case PickupType::Ammo: {
            const std::string ammoLbl = I18n::T("editor.panel.inspector.pickup.ammo_amount") + "##ppam";
            if (ImGui::SliderInt(ammoLbl.c_str(), &p.ammoAmount, 0, 999)) {
                m_editedThisFrame = true;
            }
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s", p.ammoForWeapon.c_str());
            const std::string forLbl = I18n::T("editor.panel.inspector.pickup.ammo_for") + "##ppaf";
            if (ImGui::InputText(forLbl.c_str(), buf, sizeof(buf))) {
                p.ammoForWeapon = buf;
                m_editedThisFrame = true;
            }
            break;
        }
        case PickupType::Health: {
            const std::string lbl = I18n::T("editor.panel.inspector.pickup.health_amount") + "##pph";
            if (ImGui::SliderFloat(lbl.c_str(), &p.healthAmount, 0.0f, 200.0f, "%.0f")) {
                m_editedThisFrame = true;
            }
            break;
        }
        case PickupType::Armor: {
            const std::string lbl = I18n::T("editor.panel.inspector.pickup.armor_amount") + "##ppar";
            if (ImGui::SliderFloat(lbl.c_str(), &p.armorAmount, 0.0f, 200.0f, "%.0f")) {
                m_editedThisFrame = true;
            }
            break;
        }
    }

    // Radio de pickup (siempre visible).
    const std::string radLbl = I18n::T("editor.panel.inspector.pickup.radius") + "##ppr";
    if (ImGui::SliderFloat(radLbl.c_str(), &p.pickupRadius, 0.1f, 10.0f, "%.2f m")) {
        m_editedThisFrame = true;
    }
}

} // namespace Mood
