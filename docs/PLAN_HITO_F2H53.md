# PLAN HITO F2H53 — TBD (Sub-fase 2.5 — Bloque 3 Quests, candidato)

**Estado:** plan skeleton — abrir conversación con el dev antes de implementar.

## Contexto

F2H52 cerró el Bloque 1 (Inventario) completo. La Sub-fase 2.5 sigue con:
- **Bloque 3 — Quests** (próximo lógico, predicados sobre inventory + flags).
- **Bloque 2 — Diálogos** parcialmente cerrado (F2H46/47/48/48.1 ya hechos; queda solo polish si emerge).

Lo más natural: **Quests**, porque consume el inventario (objectives `item_count(player, "key_dorada") >= 1`) y los flags de dialog (objectives `flag_set("hablo_con_npc") == true`).

## Preguntas para el dev (mecánica del jugador, NO técnica)

Antes de armar el plan detallado, conversar con el dev sobre:

1. **¿Cómo recibe el jugador una misión?**
   - Tipo Skyrim: hablás con un NPC, te dice qué hacer, aparece en el HUD.
   - Tipo Zelda: encontrás un cartel/cofre, te asignan la misión automáticamente.
   - Tipo Witcher: opciones en diálogo "acepto / no acepto".
   - **Default propuesto**: NPC le da la misión vía diálogo (`on_select_lua` ejecuta `quest.start("rescatar_gata")`).

2. **¿Qué tipos de objetivos necesita el primer juego del motor?**
   - Recolectar X items (cosume inventory).
   - Hablar con NPC Y (consume dialog flags).
   - Llegar a zona Z (necesita TriggerComponent con `area_id`).
   - Matar N enemigos de tipo T (necesita CombatComponent — no existe todavía).
   - **Decisión**: cerrar primero los 3 que ya tienen infra (collect / talk / reach), diferir los de combate a un hito de combate dedicado.

3. **¿El jugador puede tener varias misiones activas?**
   - Sí (Skyrim/RPG estándar) → Quest Log panel + tracker en HUD.
   - Solo una (Zelda clásico) → tracker simple + completar antes de la próxima.
   - **Default propuesto**: varias activas, una "tracked" que aparece en el HUD.

4. **¿Las recompensas son automáticas o el NPC se las da?**
   - Auto: al completar todos los objectives, motor da rewards.
   - Manual: el dev escribe Lua que checkea `quest.is_complete("id")` y entrega items.
   - **Default propuesto**: auto-rewards declarativos en el `.moodquest`, y `run_lua("script")` como escape hatch para lo custom.

5. **¿Necesita el Quest Editor visual del Bloque 0.1 (node graph) o un panel de formulario alcanza?**
   - Para 5-10 quests simples: panel de formulario suficiente.
   - Para quests largas con ramificación: node graph (objectives unlock chain).
   - **Default propuesto**: panel form para v1; node graph queda para Bloque 3.2 si emerge necesidad.

## Bloques propuestos (refinar tras conversación)

- **A** — `Quest::Asset` schema `.moodquest` + serialización + tests.
- **B** — `AssetManager::loadQuest` partial + `QuestAssetId`.
- **C** — `QuestSystem` runtime: state machine (Available / Active / Complete / Failed) + evaluador de predicates contra inventory/flags/counters.
- **D** — Quest Browser panel + Quest Property Editor (form-based v1).
- **E** — Lua bindings `quest.*` (`start/complete/fail/objective_complete/track/has/is_active/is_complete`).
- **F** — HUD widget tracker (extiende `ObjectiveText` de F2H41 a mostrar el quest tracked + sus objectives).
- **G** — Quest Log panel del editor (visualiza quests activos en Play Mode).
- **H** — Persistencia en `.moodmap` / save game (active quests + objective progress).
- **I** — Tour + tests + cierre.

## Salida prevista

- 1 tag (`v1.41.0-fase2-hito53`).
- ~50-80 tests nuevos (predicates + serialización + bindings).
- 1 mecánica core nueva para el jugador: aceptar/rastrear/completar misiones.
- Closes el Bloque 3 del `PLAN_SUBFASE_2_5.md`.

## Pendiente del agente al arrancar

1. Mostrar este plan al dev.
2. Hacer las 5 preguntas en lenguaje de mecánicas.
3. Refinar bloques según respuestas.
4. Plan firme → arrancar implementación.
