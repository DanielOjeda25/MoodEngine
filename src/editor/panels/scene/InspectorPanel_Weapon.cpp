// F4H2: Inspector — WeaponComponent en la categoria Gameplay.
// Cubre: dropdown del .moodweapon equipado, display del spec read-only
// (damage/range/pellets/fireRate/magSize/reload), ammo actual editable
// (debug), reload trigger.
//
// F4H2 Bloque B follow-up (pedido del dev "borrar drag, lista con
// armas"): drag-drop eliminado, reemplazado por un combo que enumera
// `AssetManager::enumerateWeapons()`. Strings hardcoded reemplazados
// por `I18n::T(...)` para que la UI cambie con el idioma.
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
#include <vector>

namespace Mood {

void InspectorPanel::renderWeaponSection(Entity e) {
    auto& w = e.getComponent<WeaponComponent>();
    if (!beginComponentSection<WeaponComponent>(e, ICON_FA_GAMEPAD " Arma")) return;

    AssetManager* assets = m_assets;
    if (assets == nullptr) {
        ImGui::TextDisabled("(AssetManager no disponible)");
        return;
    }

    // F4H2 Bloque B follow-up: combo "armas disponibles" desde
    // `enumerateWeapons` (scan defensivo de `assets/weapons/`). Reemplaza
    // el drag-drop, asi no hay que ir al AssetBrowser para equipar.
    const std::vector<AssetManager::WeaponListEntry> catalog =
        assets->enumerateWeapons(/*rescanFromDisk=*/true);

    // Preview = displayName del id actual, o "(sin arma)" si 0.
    std::string previewLabel;
    if (w.weaponAssetId == 0) {
        previewLabel = I18n::T("editor.panel.inspector.weapon.none");
    } else {
        bool found = false;
        for (const auto& entry : catalog) {
            if (entry.id == w.weaponAssetId) {
                previewLabel = entry.displayName;
                found = true;
                break;
            }
        }
        if (!found) {
            // Id presente en el componente pero no en el catalogo (raro:
            // arma borrada del disco después de equiparla). Mostrar el path
            // lógico crudo + advertencia visual.
            previewLabel = assets->weaponPathOf(w.weaponAssetId) + " (?)";
        }
    }

    const std::string equippedLabel =
        I18n::T("editor.panel.inspector.weapon.equipped") + "##weapon_combo";
    if (ImGui::BeginCombo(equippedLabel.c_str(), previewLabel.c_str())) {
        // Entrada (sin arma) — permite desequipar desde el combo sin
        // necesidad de un boton X separado.
        const std::string noneLabel =
            I18n::T("editor.panel.inspector.weapon.none");
        const bool noneSelected = (w.weaponAssetId == 0);
        if (ImGui::Selectable(noneLabel.c_str(), noneSelected)) {
            if (w.weaponAssetId != 0) {
                w.weaponAssetId = 0;
                w.currentAmmo = -1;
                m_editedThisFrame = true;
            }
        }
        if (noneSelected) ImGui::SetItemDefaultFocus();

        if (!catalog.empty()) ImGui::Separator();

        for (const auto& entry : catalog) {
            const bool selected = (entry.id == w.weaponAssetId);
            const std::string label =
                entry.displayName + "##wc_" + std::to_string(entry.id);
            if (ImGui::Selectable(label.c_str(), selected)) {
                if (entry.id != w.weaponAssetId) {
                    w.weaponAssetId = entry.id;
                    if (const Weapon::Spec* spec = assets->getWeapon(entry.id)) {
                        w.currentAmmo = static_cast<int>(spec->magazineSize);
                    }
                    m_editedThisFrame = true;
                }
            }
            if (selected) ImGui::SetItemDefaultFocus();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", entry.logicalPath.c_str());
            }
        }
        ImGui::EndCombo();
    }

    if (catalog.empty()) {
        ImGui::TextDisabled("%s",
            I18n::T("editor.panel.inspector.weapon.empty_catalog").c_str());
    }

    // --- Spec display (read-only) ---
    if (w.weaponAssetId != 0) {
        const Weapon::Spec* spec = assets->getWeapon(w.weaponAssetId);
        if (spec != nullptr) {
            ImGui::Separator();
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.weapon.spec_header").c_str());
            ImGui::Text("%s %s",
                I18n::T("editor.panel.inspector.weapon.name").c_str(),
                spec->displayName.empty()
                    ? I18n::T("editor.panel.inspector.weapon.unnamed").c_str()
                    : spec->displayName.c_str());
            ImGui::Text("%s %s",
                I18n::T("editor.panel.inspector.weapon.category").c_str(),
                spec->category.c_str());
            ImGui::Text("%s %.1f",
                I18n::T("editor.panel.inspector.weapon.damage").c_str(),
                spec->damage);
            ImGui::Text("%s %.1f m",
                I18n::T("editor.panel.inspector.weapon.range").c_str(),
                spec->range);
            ImGui::Text("%s %u  (%s %.1f°)",
                I18n::T("editor.panel.inspector.weapon.pellets").c_str(),
                spec->pellets,
                I18n::T("editor.panel.inspector.weapon.spread").c_str(),
                spec->spreadDeg);
            ImGui::Text("%s %.2f /s  (%s %.0f ms)",
                I18n::T("editor.panel.inspector.weapon.fire_rate").c_str(),
                spec->fireRatePerSec,
                I18n::T("editor.panel.inspector.weapon.cooldown").c_str(),
                1000.0f / spec->fireRatePerSec);
            ImGui::Text("%s %u",
                I18n::T("editor.panel.inspector.weapon.magazine").c_str(),
                spec->magazineSize);
            ImGui::Text("%s %.2f s",
                I18n::T("editor.panel.inspector.weapon.reload").c_str(),
                spec->reloadTimeSec);
        }
    }

    // --- Runtime state (editable para debug) ---
    ImGui::Separator();
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.weapon.runtime_header").c_str());

    int magMax = 999;
    if (w.weaponAssetId != 0) {
        if (const Weapon::Spec* spec = assets->getWeapon(w.weaponAssetId)) {
            magMax = static_cast<int>(spec->magazineSize);
        }
    }
    int displayedAmmo = (w.currentAmmo < 0) ? magMax : w.currentAmmo;
    const std::string ammoLabel =
        I18n::T("editor.panel.inspector.weapon.ammo") + "##wc_ammo";
    if (ImGui::SliderInt(ammoLabel.c_str(), &displayedAmmo, 0, magMax)) {
        w.currentAmmo = displayedAmmo;
        m_editedThisFrame = true;
    }

    if (w.fireTimer > 0.0f) {
        ImGui::Text("%s %.2f s",
            I18n::T("editor.panel.inspector.weapon.fire_cooldown").c_str(),
            w.fireTimer);
    }
    if (w.reloadTimer > 0.0f) {
        ImGui::Text("%s %.2f s",
            I18n::T("editor.panel.inspector.weapon.reloading").c_str(),
            w.reloadTimer);
    }
}

} // namespace Mood
