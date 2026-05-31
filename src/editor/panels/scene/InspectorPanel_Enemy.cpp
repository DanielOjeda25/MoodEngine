// F4H7: Inspector — EnemyComponent en la categoria Gameplay.
// Gemelo del Inspector_Weapon: combo `.moodenemy` + spec read-only +
// runtime debug (state actual + force-set state + stateTime + target).

#include "editor/panels/scene/InspectorPanel.h"
#include "editor/panels/scene/InspectorPanel_Internal.h"

#include "core/i18n/I18n.h"
#include "editor/ui/EditorUI.h"
#include "engine/assets/manager/AssetManager.h"
#include "engine/gameplay/enemy/EnemySpec.h"
#include "engine/scene/components/Components.h"

#include <imgui.h>

#include <string>
#include <vector>

namespace Mood {

namespace {

const char* stateName(EnemyState s) {
    switch (s) {
        case EnemyState::Idle:   return "Idle";
        case EnemyState::Alert:  return "Alert";
        case EnemyState::Chase:  return "Chase";
        case EnemyState::Attack: return "Attack";
        case EnemyState::Pain:   return "Pain";
        case EnemyState::Dead:   return "Dead";
    }
    return "?";
}

} // namespace

void InspectorPanel::renderEnemySection(Entity e) {
    auto& ec = e.getComponent<EnemyComponent>();
    if (!beginComponentSection<EnemyComponent>(e, ICON_FA_GAMEPAD " Enemigo")) return;

    AssetManager* assets = m_assets;
    if (assets == nullptr) {
        ImGui::TextDisabled("(AssetManager no disponible)");
        return;
    }

    // --- Combo del .moodenemy ---
    const std::vector<AssetManager::EnemyListEntry> catalog =
        assets->enumerateEnemies(/*rescanFromDisk=*/false);

    std::string previewLabel;
    if (ec.enemyAssetId == 0) {
        previewLabel = I18n::T("editor.panel.inspector.enemy.none");
    } else {
        bool found = false;
        for (const auto& entry : catalog) {
            if (entry.id == ec.enemyAssetId) {
                previewLabel = entry.displayName;
                found = true;
                break;
            }
        }
        if (!found) {
            previewLabel = assets->enemyPathOf(ec.enemyAssetId) + " (?)";
        }
    }

    const std::string equippedLabel =
        I18n::T("editor.panel.inspector.enemy.spec") + "##enemy_combo";
    if (ImGui::BeginCombo(equippedLabel.c_str(), previewLabel.c_str())) {
        const std::string noneLabel = I18n::T("editor.panel.inspector.enemy.none");
        const bool noneSelected = (ec.enemyAssetId == 0);
        if (ImGui::Selectable(noneLabel.c_str(), noneSelected)) {
            if (ec.enemyAssetId != 0) {
                ec.enemyAssetId = 0;
                m_editedThisFrame = true;
            }
        }
        if (noneSelected) ImGui::SetItemDefaultFocus();
        if (!catalog.empty()) ImGui::Separator();

        for (const auto& entry : catalog) {
            const bool selected = (entry.id == ec.enemyAssetId);
            const std::string label =
                entry.displayName + "##en_" + std::to_string(entry.id);
            if (ImGui::Selectable(label.c_str(), selected)) {
                if (entry.id != ec.enemyAssetId) {
                    ec.enemyAssetId = entry.id;
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
            I18n::T("editor.panel.inspector.enemy.empty_catalog").c_str());
    }

    // --- Spec display (read-only) ---
    if (ec.enemyAssetId != 0) {
        const Enemy::Spec* spec = assets->getEnemy(ec.enemyAssetId);
        if (spec != nullptr) {
            ImGui::Separator();
            ImGui::TextDisabled("%s",
                I18n::T("editor.panel.inspector.enemy.spec_header").c_str());
            ImGui::Text("%s %s",
                I18n::T("editor.panel.inspector.enemy.name").c_str(),
                spec->displayName.empty()
                    ? I18n::T("editor.panel.inspector.enemy.unnamed").c_str()
                    : spec->displayName.c_str());
            ImGui::Text("%s %.0f HP",
                I18n::T("editor.panel.inspector.enemy.health").c_str(),
                spec->health);
            ImGui::Text("%s %.1f m",
                I18n::T("editor.panel.inspector.enemy.aggro_range").c_str(),
                spec->aggroRange);
            ImGui::Text("%s %.1f m",
                I18n::T("editor.panel.inspector.enemy.attack_range").c_str(),
                spec->attackRange);
            ImGui::Text("%s %.1f m/s",
                I18n::T("editor.panel.inspector.enemy.move_speed").c_str(),
                spec->moveSpeed);
            ImGui::Text("%s %.1f",
                I18n::T("editor.panel.inspector.enemy.damage").c_str(),
                spec->damage);
            ImGui::Text("%s %.2f s",
                I18n::T("editor.panel.inspector.enemy.attack_cooldown").c_str(),
                spec->attackCooldown);
            ImGui::Text("%s %.1f / %.2f s",
                I18n::T("editor.panel.inspector.enemy.pain").c_str(),
                spec->painThreshold, spec->painDuration);
        }
    }

    // --- Runtime debug ---
    ImGui::Separator();
    ImGui::TextDisabled("%s",
        I18n::T("editor.panel.inspector.enemy.runtime_header").c_str());

    // Force-set state (debug).
    int curStateIdx = static_cast<int>(ec.state);
    const char* stateLabels[] = {"Idle", "Alert", "Chase", "Attack", "Pain", "Dead"};
    const std::string stateComboLabel =
        I18n::T("editor.panel.inspector.enemy.state") + "##enemy_state";
    if (ImGui::Combo(stateComboLabel.c_str(), &curStateIdx, stateLabels, 6)) {
        ec.state = static_cast<EnemyState>(curStateIdx);
        ec.stateTime = 0.0f;
        m_editedThisFrame = true;
    }

    ImGui::Text("%s %s",
        I18n::T("editor.panel.inspector.enemy.current_state").c_str(),
        stateName(ec.state));
    ImGui::Text("%s %.2f s",
        I18n::T("editor.panel.inspector.enemy.state_time").c_str(),
        ec.stateTime);
    ImGui::Text("%s %u",
        I18n::T("editor.panel.inspector.enemy.target").c_str(),
        ec.targetEntity);
    ImGui::Text("%s %d",
        I18n::T("editor.panel.inspector.enemy.pains_total").c_str(),
        ec.painsTotal);
}

} // namespace Mood
