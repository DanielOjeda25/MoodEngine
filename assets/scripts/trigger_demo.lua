-- Hito 33: demo de TriggerComponent. Loguea cuando el jugador entra
-- y sale del AABB del trigger. Usar como template para puertas,
-- kill volumes, zonas de musica, etc.

function on_trigger_enter()
    log.info("[trigger] player entro")
end

function on_trigger_exit()
    log.info("[trigger] player salio")
end
