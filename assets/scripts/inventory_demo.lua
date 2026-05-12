-- Demo del sistema de inventario (F2H52). Conecta la mecanica del motor
-- con semantica game-specific via los hooks Lua. Engine-grade: el motor
-- mueve items entre containers, este script decide que pasa cuando los
-- usas / tiras / levantas.
--
-- API ejercitada:
--   - inventory.on_pickup(cb)  -- jugador levanto algo del mundo (E)
--   - inventory.on_drop(cb)    -- algo salio del inventario al mundo
--   - inventory.on_use(cb)     -- click derecho > Usar en el panel
--   - inventory.has(path, n)   -- query player-implicit
--   - inventory.count(path)
--   - inventory.add(entity, path, n)
--   - inventory.remove(entity, path, n)
--
-- Items esperados (deben existir en assets/items/):
--   - health_potion.mooditem -> hereda stat heal=20, max_stack=10
--   - mana_potion.mooditem   -> stat heal=10 (mana_pool si lo agregas)
--   - iron_sword.mooditem    -> tags={"weapon"}, stat damage=10
--
-- Si tus items se llaman distinto, ajusta los paths abajo. Engine NO los
-- crea — son contenido del juego.

local INITIALIZED = false

-- Mapa de comportamiento al usar un item. La key es el path; el value
-- es una funcion que recibe (entity, path) y decide si consume + el
-- efecto en HUD/world. Engine-grade: el motor no conoce "potion heals
-- HP" — esa relacion vive aca.
local USE_HANDLERS = {

    ["items/health_potion.mooditem"] = function(ent, path)
        local cur = hud.getHp()
        local max = hud.getMaxHp()
        if cur >= max then
            log.info("[inv_demo] Health full, potion uso no consume")
            return false  -- no consumir
        end
        local healed = math.min(max, cur + 20)
        hud.setHp(healed)
        hud.pushPickup("Healed +" .. (healed - cur) .. " HP")
        -- Auto-consume: el motor NO lo hace, lo decide este script.
        inventory.remove(ent, path, 1)
        return true
    end,

    ["items/mana_potion.mooditem"] = function(ent, path)
        hud.pushPickup("Mana restored (+10)")
        inventory.remove(ent, path, 1)
        return true
    end,

    ["items/iron_sword.mooditem"] = function(ent, path)
        -- Caso "equipable": no se consume. Solo cambia el objetivo del HUD
        -- como feedback visual de que esta equipado.
        hud.setObjective("Equipped: Iron Sword")
        log.info("[inv_demo] Sword equipped (no consume)")
        return true
    end,
}

function onUpdate(self, dt)
    if INITIALIZED then return end
    INITIALIZED = true

    -- on_pickup: el motor ya muestra "+ Iron Sword x1" en el HUD; este
    -- callback solo loggea + dispara efectos game-specific (ej. tutorial
    -- "primer item levantado"). Engine-grade: el callback es OPCIONAL,
    -- el motor sigue funcionando si nadie registra.
    inventory.on_pickup(function(item_path, qty)
        log.info(string.format("[inv_demo] picked up %s x%d", item_path, qty))
        -- Demo: si el primer item levantado es una pocion, hint al user.
        if item_path == "items/health_potion.mooditem"
           and inventory.count(item_path) == qty then
            hud.setObjective("Tab opens inventory. Right-click an item.")
        end
    end)

    -- on_drop: cuando el usuario "Tira" un item desde el HUD context menu,
    -- el motor remueve del inv + spawnea pickup en el mundo + invoca este
    -- callback. Demo solo loggea.
    inventory.on_drop(function(item_path, qty)
        log.info(string.format("[inv_demo] dropped %s x%d", item_path, qty))
    end)

    -- on_use: dispatch al handler segun el path. Si no hay handler, log
    -- + no consumir. El motor NO auto-consume — el dev decide siempre.
    inventory.on_use(function(entity, item_path)
        local handler = USE_HANDLERS[item_path]
        if handler then
            handler(entity, item_path)
        else
            log.warn("[inv_demo] no use handler for " .. item_path)
        end
    end)

    -- HUD inicial. El widget inventory_panel arranca OFF; el player lo
    -- abre con Tab. El objetivo guia el primer uso.
    hud.setMaxHp(100)
    hud.setHp(80)  -- menos del max para que la pocion tenga efecto
    hud.setObjective("Walk over items to pick them up (E)")

    log.info("[inv_demo] hooks registered (on_pickup/on_drop/on_use)")
end
