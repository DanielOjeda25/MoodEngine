#include "engine/dialog/DialogInteractSystem.h"

#include "engine/assets/manager/AssetManager.h"
#include "engine/dialog/DialogAsset.h"
#include "engine/dialog/DialogSystem.h"
#include "engine/game/state/GameState.h"
#include "core/i18n/I18n.h"
#include "engine/scene/components/Components.h"
#include "engine/scene/core/Entity.h"
#include "engine/scene/core/Scene.h"

namespace Mood::Dialog::DialogInteractSystem {

namespace {
// Flag para distinguir "interact_prompt seteado por este sistema" de
// uno seteado por Lua (script.set_interact). Cuando el player sale del
// trigger Y nosotros habiamos seteado el prompt, lo limpiamos. Si Lua
// lo sobrescribe entre medio, aceptamos no detectarlo (v1).
bool g_lastSetByDialog = false;
} // namespace

bool tick(Scene& scene, AssetManager& assets, bool eJustPressed) {
    auto& hud = Mood::GameState::hud();
    bool anyPromptShown = false;
    bool dialogStartedThisFrame = false;

    scene.forEach<TriggerComponent, DialogComponent>(
        [&](Entity, TriggerComponent& tr, DialogComponent& dc) {
            if (!dc.autoStartOnInteract) return;
            if (!tr.playerInside) return;
            // Dialog ya activo: no proponer otro start y NO mostrar prompt
            // (estamos hablando con alguien — no tiene sentido sugerir "[E]
            // Hablar" mientras hay un dialog en curso).
            if (DialogSystem::isActive()) return;

            // Setear prompt (idempotente — escribir el mismo string).
            hud.interact_prompt = Mood::I18n::T("hud.dialog.interact_prompt");
            g_lastSetByDialog = true;
            anyPromptShown = true;

            if (eJustPressed) {
                // Cargar asset si todavia no cacheado en el componente.
                if (dc.cachedDialogId == 0 && !dc.dialogPath.empty()) {
                    dc.cachedDialogId = assets.loadDialog(dc.dialogPath);
                }
                const Asset* asset = (dc.cachedDialogId != 0)
                    ? assets.getDialog(dc.cachedDialogId)
                    : nullptr;
                if (asset != nullptr && DialogSystem::start(asset)) {
                    // Dialog arranco: limpiar el prompt (sino quedaria
                    // visible detras de la caja de dialogo HL2).
                    hud.interact_prompt.clear();
                    g_lastSetByDialog = false;
                    anyPromptShown = false;
                    dialogStartedThisFrame = true;
                }
            }
        });

    // Si el player salio de todos los triggers Y el prompt fue seteado
    // por nosotros, limpiarlo. Lua-set prompts no se tocan (heuristica
    // simple — Lua puede regularlos en sus propios callbacks).
    if (!anyPromptShown && g_lastSetByDialog) {
        hud.interact_prompt.clear();
        g_lastSetByDialog = false;
    }
    return dialogStartedThisFrame;
}

void tickActiveDialog(bool eJustPressed, int digitJustPressed) {
    if (!DialogSystem::isActive()) return;

    const auto choices = DialogSystem::availableChoices();
    if (choices.empty()) {
        // Continue-only: E avanza por el unico output.
        if (eJustPressed) {
            DialogSystem::continueNext();
        }
        return;
    }
    // Hay choices: digit 1..N (mapeo a choiceIndex en availableChoices).
    if (digitJustPressed >= 1
        && static_cast<size_t>(digitJustPressed) <= choices.size()) {
        const int idx = choices[digitJustPressed - 1].choiceIndex;
        DialogSystem::advance(idx);
    }
}

} // namespace Mood::Dialog::DialogInteractSystem
