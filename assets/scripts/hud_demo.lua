-- Script demo del Hito 20 + F2H39 + F2H41: ejercita los 13 widgets del HUD.
--
-- API actual (post-F2H41):
--   Health:     hud.setHp / hud.setMaxHp / getters
--   Ammo:       hud.setMag / hud.setMaxMag / hud.setReserve / getters
--   Stamina:    hud.setStamina / hud.setMaxStamina / getters    [F2H41]
--   Pause:      hud.setPaused / hud.getPaused
--   Interact:   hud.setInteractPrompt / hud.clearInteractPrompt
--   Objective:  hud.setObjective / hud.clearObjective           [F2H41]
--   Efectos:    hud.showHitMarker / hud.flashDamage(x,y) /
--               hud.pushPickup(text) / hud.pushKill(text) /     [F2H41]
--               hud.pushKillColored(text, r, g, b)              [F2H41]
--   Toggles:    hud.setWidget("name", bool) / hud.isWidgetEnabled("name")
--
-- Comportamiento del demo (post-F2H41):
--   - Init: HP=100/100, MAG=12/30, RESERVE=90, STAMINA=100/100,
--     OBJECTIVE="Demo: explorar el mapa de pruebas".
--   - Cada 1.0s drena 1 HP (al llegar a 0 pausa el juego).
--   - Cada 1.5s drena 2 stamina, regen lento si > 0.
--   - Cada 3.0s flashDamage random + showHitMarker.
--   - Cada 5.0s pushPickup.
--   - Cada 7.0s toggle interactPrompt.
--   - Cada 8.0s pushKill rotativo (Headcrab / Manhack / Combine / Antlion).
--   - Cada 12.0s toggle CRT scanline overlay.

local INITIALIZED = false
local accumHp     = 0.0
local accumStam   = 0.0
local accumDmg    = 0.0
local accumPickup = 0.0
local accumPrompt = 0.0
local accumKill   = 0.0
local accumScan   = 0.0
local pickupCount = 0
local killCount   = 0
local promptOn    = false
local scanOn      = false

-- Lista rotativa de enemigos para el kill feed (HL flavor).
local KILL_ENEMIES = {"Headcrab", "Manhack", "Combine", "Antlion", "Zombie"}

function onUpdate(self, dt)
    if not INITIALIZED then
        hud.setMaxHp(100)
        hud.setHp(100)
        hud.setMag(12)
        hud.setMaxMag(30)
        hud.setReserve(90)
        hud.setMaxStamina(100)
        hud.setStamina(100)
        hud.clearInteractPrompt()
        hud.setObjective("Demo: explore the test map")
        log.info("hud_demo (F2H41): HP=100, MAG=12/30, STAM=100, OBJ='explore'")
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

    -- Stamina drain/regen cada 1.5s. Drena 2 si > 30, regen 5 si <= 30.
    accumStam = accumStam + dt
    if accumStam >= 1.5 then
        accumStam = accumStam - 1.5
        local s = hud.getStamina()
        local maxS = hud.getMaxStamina()
        if s > 30 then
            hud.setStamina(math.max(0, s - 2))
        else
            hud.setStamina(math.min(maxS, s + 5))
        end
    end

    -- Damage flash + hit marker cada 3s.
    accumDmg = accumDmg + dt
    if accumDmg >= 3.0 then
        accumDmg = accumDmg - 3.0
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
            hud.setInteractPrompt("[E] Pick up demo item")
            promptOn = true
        end
    end

    -- Kill feed cada 8s con rotacion + colores.
    accumKill = accumKill + dt
    if accumKill >= 8.0 then
        accumKill = accumKill - 8.0
        killCount = killCount + 1
        local enemy = KILL_ENEMIES[((killCount - 1) % #KILL_ENEMIES) + 1]
        hud.pushKill("Killed: " .. enemy .. " #" .. killCount)
    end

    -- Toggle CRT scanline overlay cada 12s.
    accumScan = accumScan + dt
    if accumScan >= 12.0 then
        accumScan = accumScan - 12.0
        scanOn = not scanOn
        hud.setWidget("crt_scanline", scanOn)
        log.info("hud_demo: crt_scanline = " .. tostring(scanOn))
    end
end
