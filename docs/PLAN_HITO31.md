# Plan — Hito 31: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 30 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 30 cerrado** (`v0.30.0-hito30`, suite **264/5494**). Player Character Controller con Jolt: capsule + WASD + jump + crouch. Primer FPS-movement real.

El Hito 31 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

**Recordatorio importante:** networking / multijugador está **EXPLÍCITAMENTE fuera de alcance** desde el doc técnico (`MOODENGINE_CONTEXTO_TECNICO.md` sección "Fuera de alcance"). NO proponerlo como candidato.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Raycasts + Triggers expuestos a Lua

**Por qué:** combo natural post-Hito 30. Jolt soporta raycasts y body sensors/triggers. Habilitarlos abre las dos features grandes pendientes para gameplay scriptable:
1. **Raycast desde Lua**: armas (line-of-sight + hitscan), enemigos que ven al jugador, picking físico, interacciones a distancia. `physics.raycast(origin, dir, maxDist) → (hit, point, normal, entity)`.
2. **OnTriggerEnter/Exit**: zonas que detectan entrada sin colisión sólida (puertas automáticas, kill volumes, checkpoints, gatillos de scripts). `TriggerComponent { halfExtents }` + callback Lua `on_enter(other)` / `on_exit(other)`.

**Coste:** medio. API en `PhysicsWorld` + bindings en `LuaBindings::physics` + `TriggerComponent` con dispatch al ScriptSystem.

**Trigger ideal:** ahora — el char controller del Hito 30 + raycasts/triggers = gameplay scriptable real (FPS con armas + zonas).

### B. Save/Load de gameplay state

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego. Save & load = primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### C. Polish del NavAgent (rotación + persistencia)

**Por qué:** pendiente diferido del Hito 23 + 25 + 26 + 27 + 30. Hoy CesiumMan camina sin girar, y el NavAgent no se persiste en `.moodmap`.

**Coste:** bajo. Un par de horas máximo.

**Trigger ideal:** combo con A si el dev quiere armar un demo con enemigos persistidos + raycasts (ej. enemigo que dispara al ver al jugador).

### D. Fricción + headbob + animación crouch (polish del Hito 30)

**Por qué:** los pendientes menores del Hito 30. Caja física se desliza demasiado al ser empujada (fricción default baja en Jolt). Cámara sin animación de paso (headbob) se siente rígida. Crouch instantáneo (1 frame) podría lerping a 0.2s.

**Coste:** bajo, todos juntos. Es polish puro.

**Trigger ideal:** dev se queja del feel actual.

### E. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con B (sin save/load no hay "Load Game").

### F. Particle system polish (sorting + local space)

**Por qué:** pendientes menores del Hito 29. Sort back-to-front + flag `localSpace` para humo siguiendo entidades.

**Coste:** bajo.

**Trigger ideal:** dev nota artifacts visuales o pide humo following.

---

## Diferidos (revisar más adelante)

### InspectorEditCommand (genérico) — del Hito 28

51 widgets en 511 líneas requieren un patrón templado. **Trigger:** dev pide undo de sliders.

### Commands para drops modificadores — del Hito 28

Texture / material / script drops sin history. **Trigger:** combo con InspectorEditCommand.

### Handle remap en HistoryStack — del Hito 27

edit→delete→undo→undo deja segundo undo no-op silencioso.

### PackageBuilder smart-pack — del Hito 26

Solo copiar assets referenciados. **Trigger:** demo packageado > 50 MB.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
