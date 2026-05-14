-- Demo del sistema de quests (F2H53). Conecta:
--   - QuestSystem state machine + tick auto-evalua predicates.
--   - HUD widget `objective_text` re-purposed como Quest Tracker.
--   - Bindings Lua tabla `quest`.
--
-- Para verlo funcionando:
--   1) Abrir o crear un mapa con:
--      - 1 entity con TagComponent.name = "Player" + InventoryComponent.
--      - 1 entity con ScriptComponent apuntando a este archivo.
--   2) Asegurar que `assets/items/mysterious_key.mooditem` existe (ya viene
--      en el repo, F2H51).
--   3) Play. El script spawnea el pickup adelante del player. Caminás
--      hasta el cubo, presionás E, y deberias ver el objective tacharse
--      en el tracker del HUD + el log de Quest completada.

local INITIALIZED = false
local QUEST_PATH = "quests/quest_demo.moodquest"
local KEY_PATH   = "items/mysterious_key.mooditem"

function onUpdate(self, dt)
    if INITIALIZED then return end
    INITIALIZED = true

    -- Hooks game-side: el motor solo cambia de estado y aplica rewards;
    -- estos callbacks son donde el dev hace XP/SFX/UI/cinematics.
    quest.on_start(function(path)
        log.info("[quest_demo] start: " .. path)
    end)

    quest.on_complete(function(path)
        log.info("[quest_demo] COMPLETA: " .. path)
        -- Feedback HUD via pickup toast (reusable).
        hud.pushPickup("Quest completada!")
        -- Reward del .moodquest fue `dialog.set_var("quest_demo_done", "yes")` —
        -- el motor ya la aplicó via executor. Confirmar:
        local v = dialog.get_var("quest_demo_done")
        log.info("[quest_demo] reward var quest_demo_done='" .. v .. "'")
    end)

    quest.on_fail(function(path)
        log.warn("[quest_demo] fail: " .. path)
    end)

    -- Spawn del item pickup adelante del player. spawn_pickup crea una
    -- entity con Trigger + ItemPickupComponent + MeshRenderer (cubo +
    -- missing texture). Engine-grade: el motor solo materializa la
    -- entity; la semantica de "que pasa al levantarlo" se la da el
    -- ItemPickupSystem (ya cableado por F2H52).
    local ok_spawn = inventory.spawn_pickup(KEY_PATH, 2.0, 1.0, 2.0, 1)
    if ok_spawn then
        log.info("[quest_demo] spawned pickup at (2, 1, 2)")
    else
        log.warn(
            "[quest_demo] spawn_pickup fallo. " ..
            "Chequea que exista una entity con tag 'Player' + InventoryComponent.")
    end

    -- Arrancar el quest y trackearlo. set_tracked hace que el HUD widget
    -- `objective_text` lo muestre con header amarillo + lista de objectives.
    local started = quest.start(QUEST_PATH)
    if started then
        quest.set_tracked(QUEST_PATH)
        log.info("[quest_demo] quest arrancada y trackeada")
    else
        log.warn("[quest_demo] quest.start fallo. Chequea el path: " .. QUEST_PATH)
    end

    -- Fallback HUD por si el player no agrega Player+InventoryComponent.
    hud.setMaxHp(100)
    hud.setHp(100)
end
