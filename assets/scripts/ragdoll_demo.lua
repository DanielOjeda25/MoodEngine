-- F2H66 demo: trigger ragdoll. Cuando el player entra al volumen del
-- trigger, el NPC asociado (tag "NPC" en `narrative_demo.moodmap`)
-- transiciona a Ragdolling con un impulse hacia atras-arriba (estilo
-- HL2: como si lo golpearan al pecho).
--
-- Una vez en ragdoll, no se vuelve a Animated (convencion HL2). Para
-- volver a usar el NPC, recargar el mapa.

function on_trigger_enter()
    if ragdoll.is_ragdolling("NPC") then
        -- Ya esta ragdolleando — no-op (el player puede re-entrar al volumen).
        return
    end
    log.info("[ragdoll_demo] player entro -> NPC se desploma")
    ragdoll.enable("NPC", {x = 0, y = 80, z = -40})
end

function on_trigger_exit()
    -- No-op. El NPC ya esta floppando o sigue animado.
end
