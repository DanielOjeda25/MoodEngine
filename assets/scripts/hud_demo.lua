-- Script demo del Hito 20 Bloque 5: ejercita la tabla `hud` que
-- expone GameState al runtime de scripts.
--
-- API:
--   hud.setHp(int) / hud.setAmmo(int) / hud.setPaused(bool)
--   hud.getHp() / hud.getAmmo() / hud.getPaused()
--
-- Comportamiento:
--   - Al entrar al Play Mode (primer frame del script), inicializa HP=75
--     y Ammo=12 — visibles en los bloques del HUD.
--   - Cada 1s drena 1 punto de HP. Cuando HP llega a 0, pausa el juego
--     (efecto "game over modal").

local accumulator = 0.0
local INITIALIZED = false

function onUpdate(self, dt)
    if not INITIALIZED then
        hud.setHp(75)
        hud.setAmmo(12)
        log.info("hud_demo: HP=" .. hud.getHp() .. " AMMO=" .. hud.getAmmo())
        INITIALIZED = true
    end

    accumulator = accumulator + dt
    if accumulator >= 1.0 then
        accumulator = accumulator - 1.0
        local hp = hud.getHp()
        if hp > 0 then
            hud.setHp(hp - 1)
        else
            -- Game over: forzamos pausa para mostrar el menu modal.
            if not hud.getPaused() then
                hud.setPaused(true)
                log.info("hud_demo: HP=0 — pausa forzada por script")
            end
        end
    end
end
