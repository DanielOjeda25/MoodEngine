// F4H2 + F4H3: Inspector — WeaponComponent en la categoria Gameplay.
//
// F4H2 (Bloque A): un solo slot — combo `.moodweapon` + spec read-only
// + slider ammo runtime.
// F4H2 Bloque B follow-up: drag-drop reemplazado por combo desde
// `enumerateWeapons()`. Strings via `I18n::T(...)`.
// F4H3: multi-slot. 4 tabs (uno por slot del arsenal) + indicador del
// slot activo + boton "Set active" por slot. Cada tab muestra el combo
// + spec + ammo del slot seleccionado para editar (que NO siempre es el
// slot activo en runtime).
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

namespace {

// Renderiza el cuerpo del slot dado (combo + spec + ammo). Comparte
// codigo entre los 4 tabs.
void renderSlotBody(WeaponComponent& w, u32 slotIdx, AssetManager& assets,
                     bool& edited) {
    WeaponSlot& s = w.slots[slotIdx];

    const std::vector<AssetManager::WeaponListEntry> catalog =
        assets.enumerateWeapons(/*rescanFromDisk=*/false);

    // Preview = displayName del id actual, o "(sin arma)" si 0.
    std::string previewLabel;
    if (s.weaponAssetId == 0) {
        previewLabel = I18n::T("editor.panel.inspector.weapon.none");
    } else {
        bool found = false;
        for (const auto& entry : catalog) {
            if (entry.id == s.weaponAssetId) {
                previewLabel = entry.displayName;
                found = true;
                break;
            }
        }
        if (!found) {
            previewLabel = assets.weaponPathOf(s.weaponAssetId) + " (?)";
        }
    }

    const std::string equippedLabel =
        I18n::T("editor.panel.inspector.weapon.equipped")
        + "##weapon_combo_" + std::to_string(slotIdx);
    if (ImGui::BeginCombo(equippedLabel.c_str(), previewLabel.c_str())) {
        const std::string noneLabel =
            I18n::T("editor.panel.inspector.weapon.none");
        const bool noneSelected = (s.weaponAssetId == 0);
        if (ImGui::Selectable(noneLabel.c_str(), noneSelected)) {
            if (s.weaponAssetId != 0) {
                s.weaponAssetId = 0;
                s.currentAmmo = -1;
                edited = true;
            }
        }
        if (noneSelected) ImGui::SetItemDefaultFocus();

        if (!catalog.empty()) ImGui::Separator();

        for (const auto& entry : catalog) {
            const bool selected = (entry.id == s.weaponAssetId);
            const std::string label =
                entry.displayName + "##wc_" + std::to_string(slotIdx)
                + "_" + std::to_string(entry.id);
            if (ImGui::Selectable(label.c_str(), selected)) {
                if (entry.id != s.weaponAssetId) {
                    s.weaponAssetId = entry.id;
                    if (const Weapon::Spec* spec = assets.getWeapon(entry.id)) {
                        s.currentAmmo = static_cast<int>(spec->magazineSize);
                    }
                    edited = true;
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
    if (s.weaponAssetId != 0) {
        const Weapon::Spec* spec = assets.getWeapon(s.weaponAssetId);
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
    if (s.weaponAssetId != 0) {
        if (const Weapon::Spec* spec = assets.getWeapon(s.weaponAssetId)) {
            magMax = static_cast<int>(spec->magazineSize);
        }
    }
    int displayedAmmo = (s.currentAmmo < 0) ? magMax : s.currentAmmo;
    const std::string ammoLabel =
        I18n::T("editor.panel.inspector.weapon.ammo")
        + "##wc_ammo_" + std::to_string(slotIdx);
    if (ImGui::SliderInt(ammoLabel.c_str(), &displayedAmmo, 0, magMax)) {
        s.currentAmmo = displayedAmmo;
        edited = true;
    }
}

} // namespace

void InspectorPanel::renderWeaponSection(Entity e) {
    auto& w = e.getComponent<WeaponComponent>();
    if (!beginComponentSection<WeaponComponent>(e, ICON_FA_GAMEPAD " Arma")) return;

    AssetManager* assets = m_assets;
    if (assets == nullptr) {
        ImGui::TextDisabled("(AssetManager no disponible)");
        return;
    }

    // F4H3 multi-slot UI: tab bar con los 4 slots + indicador del activo.
    // El slot "viewed" (el tab seleccionado) puede ser distinto del slot
    // "active" (el que se dispara) — el dev puede editar un slot mientras
    // otro esta activo.
    if (ImGui::BeginTabBar("##weapon_slots")) {
        for (u32 i = 0; i < WeaponComponent::k_maxSlots; ++i) {
            const bool isActive = (i == w.activeSlot);
            // Etiqueta: "1" + estrella si activo.
            std::string label = std::to_string(i + 1);
            if (isActive) label += "*";
            label += "##slot" + std::to_string(i);
            if (ImGui::BeginTabItem(label.c_str())) {
                if (isActive) {
                    ImGui::TextDisabled("%s",
                        I18n::T("editor.panel.inspector.weapon.active_slot").c_str());
                } else {
                    if (ImGui::SmallButton(
                            (I18n::T("editor.panel.inspector.weapon.set_active")
                             + "##setactive_" + std::to_string(i)).c_str())) {
                        w.lastActiveSlot = w.activeSlot;
                        w.activeSlot = i;
                        w.fireTimer = 0.0f;
                        w.reloadTimer = 0.0f;
                        m_editedThisFrame = true;
                    }
                }
                ImGui::Separator();

                bool edited = false;
                renderSlotBody(w, i, *assets, edited);
                if (edited) m_editedThisFrame = true;

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

    // Timers del frame (compartidos por slot activo).
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
