-- Script demo del Hito 20 Bloque 5 + F2H39: ejercita el HUD framework.
--
-- API actual (post-F2H39):
--   Health: hud.setHp / hud.setMaxHp / hud.getHp / hud.getMaxHp
--   Ammo:   hud.setMag / hud.setMaxMag / hud.setReserve / getters
--   Pause:  hud.setPaused / hud.getPaused
--   Interact: hud.setInteractPrompt / hud.clearInteractPrompt
--   Efectos: hud.showHitMarker / hud.flashDamage(x,y) / hud.pushPickup(text)
--   Toggles: hud.setWidget("name", bool) / hud.isWidgetEnabled("name")
--
-- Comportamiento del demo:
--   - Init: HP=100/100, MAG=12/30, Reserve=90.
--   - Cada 1.0s drena 1 HP (al llegar a 0 pausa el juego).
--   - Cada 3.0s flashDamage desde una direccion aleatoria + showHitMarker
--     (asi el dev ve los dos efectos sin necesidad de gameplay real).
--   - Cada 5.0s push de un pickup notification de demo.
--   - Cada 7.0s toggle del interact prompt (set / clear).

local INITIALIZED = false
local accumHp = 0.0
local accumDmg = 0.0
local accumPickup = 0.0
local accumPrompt = 0.0
local pickupCount = 0
local promptOn = false

function onUpdate(self, dt)
    if not INITIALIZED then
        hud.setMaxHp(100)
        hud.setHp(100)
        hud.setMag(12)
        hud.setMaxMag(30)
        hud.setReserve(90)
        hud.clearInteractPrompt()
        log.info("hud_demo (F2H39): HP=100, MAG=12/30, RESERVE=90")
        INITIALIZED = true
    end

    -- HP drain cada 1s.
    accumHp = accumHp + dt
    if accumHp >= 1.0 then
        accumHp = accumHp - 1.0
        local hp = hud.getHp()
        if hp > 0 then
            hud.setHp(hp - 1)
        else
            if not hud.getPaused() then
                hud.setPaused(true)
                log.info("hud_demo: HP=0 - pausa forzada por script")
            end
        end
    end

    -- Damage flash + hit marker cada 3s.
    accumDmg = accumDmg + dt
    if accumDmg >= 3.0 then
        accumDmg = accumDmg - 3.0
        -- Direccion pseudo-aleatoria via math.random.
        local angle = math.random() * 2.0 * math.pi
        local dx = math.cos(angle)
        local dy = math.sin(angle)
        hud.flashDamage(dx, dy)
        hud.showHitMarker()
    end

    -- Pickup notification cada 5s.
    accumPickup = accumPickup + dt
    if accumPickup >= 5.0 then
        accumPickup = accumPickup - 5.0
        pickupCount = pickupCount + 1
        hud.pushPickup("Picked up: Demo Item #" .. pickupCount)
    end

    -- Toggle interact prompt cada 7s.
    accumPrompt = accumPrompt + dt
    if accumPrompt >= 7.0 then
        accumPrompt = accumPrompt - 7.0
        if promptOn then
            hud.clearInteractPrompt()
            promptOn = false
        else
            hud.setInteractPrompt("[E] Levantar item demo")
            promptOn = true
        end
    end
end
