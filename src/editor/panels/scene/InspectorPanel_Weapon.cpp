// F4H2: Inspector — WeaponComponent en la categoria Gameplay.
// Cubre: slot del .moodweapon equipado (drag-drop + clear), display
// del spec read-only (damage/range/pellets/fireRate/magSize/reload),
// ammo actual editable (debug), reload trigger button.
//
// Engine-generic: el panel NUNCA muestra nombres de armas hardcoded —
// todo viene del Spec via AssetManager.

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/weapon/WeaponSpec.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <string>

namespace Mood {

void InspectorPanel::renderWeaponSection(Entity e) {
    auto& w = e.getComponent<WeaponComponent>();
    if (!beginComponentSection<WeaponComponent>(e, ICON_FA_GAMEPAD " Arma")) return;

    AssetManager* assets = m_assets;
    if (assets == nullptr) {
        ImGui::TextDisabled("(AssetManager no disponible)");
        return;
    }

    // --- Slot del .moodweapon equipado ---
    const std::string pathLabel = (w.weaponAssetId != 0)
        ? assets->weaponPathOf(w.weaponAssetId)
        : std::string("(sin arma)");

    ImGui::Text("Equipped");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
                           ImVec4(0.20f, 0.20f, 0.25f, 1.0f));
    ImGui::Button(pathLabel.c_str(),
                   ImVec2(ImGui::CalcItemWidth(), 0));
    ImGui::PopStyleColor();

    // Drag-drop target: aceptar payload "MOOD_WEAPON_ASSET" (u32 ID)
    // o "MOOD_ASSET_PATH" (string path generico del AssetBrowser).
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("MOOD_WEAPON_ASSET")) {
            const u32 droppedId = *static_cast<const u32*>(payload->Data);
            if (droppedId != w.weaponAssetId) {
                w.weaponAssetId = droppedId;
                // Reset ammo a magsize del nuevo spec.
                if (const Weapon::Spec* spec = assets->getWeapon(droppedId)) {
                    w.currentAmmo = static_cast<int>(spec->magazineSize);
                }
                m_editedThisFrame = true;
            }
        }
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("MOOD_ASSET_PATH")) {
            const char* path = static_cast<const char*>(payload->Data);
            // Solo aceptar si termina en .moodweapon.
            const std::string p(path, payload->DataSize);
            if (p.size() >= 11
                && p.substr(p.size() - 11) == ".moodweapon") {
                const u32 newId = assets->loadWeapon(p);
                if (newId != w.weaponAssetId) {
                    w.weaponAssetId = newId;
                    if (const Weapon::Spec* spec = assets->getWeapon(newId)) {
                        w.currentAmmo = static_cast<int>(spec->magazineSize);
                    }
                    m_editedThisFrame = true;
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Clear button.
    ImGui::SameLine();
    if (ImGui::SmallButton("X##weapon_clear")) {
        w.weaponAssetId = 0;
        w.currentAmmo = -1;
        m_editedThisFrame = true;
    }

    // --- Spec display (read-only) ---
    if (w.weaponAssetId != 0) {
        const Weapon::Spec* spec = assets->getWeapon(w.weaponAssetId);
        if (spec != nullptr) {
            ImGui::Separator();
            ImGui::TextDisabled("Spec del arma (read-only)");
            ImGui::Text("Nombre:    %s",
                spec->displayName.empty() ? "(unnamed)" : spec->displayName.c_str());
            ImGui::Text("Categoria: %s", spec->category.c_str());
            ImGui::Text("Damage:    %.1f", spec->damage);
            ImGui::Text("Range:     %.1f m", spec->range);
            ImGui::Text("Pellets:   %u  (spread %.1f deg)",
                spec->pellets, spec->spreadDeg);
            ImGui::Text("Fire rate: %.2f /s  (cooldown %.0f ms)",
                spec->fireRatePerSec, 1000.0f / spec->fireRatePerSec);
            ImGui::Text("Magazine:  %u", spec->magazineSize);
            ImGui::Text("Reload:    %.2f s", spec->reloadTimeSec);
        }
    }

    // --- Runtime state (editable para debug) ---
    ImGui::Separator();
    ImGui::TextDisabled("Estado runtime");

    // Ammo: int slider con range del magazine.
    int magMax = 999;
    if (w.weaponAssetId != 0) {
        if (const Weapon::Spec* spec = assets->getWeapon(w.weaponAssetId)) {
            magMax = static_cast<int>(spec->magazineSize);
        }
    }
    int displayedAmmo = (w.currentAmmo < 0) ? magMax : w.currentAmmo;
    if (ImGui::SliderInt("Ammo##wc_ammo", &displayedAmmo, 0, magMax)) {
        w.currentAmmo = displayedAmmo;
        m_editedThisFrame = true;
    }

    if (w.fireTimer > 0.0f) {
        ImGui::Text("Fire cooldown: %.2f s", w.fireTimer);
    }
    if (w.reloadTimer > 0.0f) {
        ImGui::Text("Reloading: %.2f s", w.reloadTimer);
    }
}

} // namespace Mood
