# Plan — Hito 31: Polish del feel (char controller + particles)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 31 cerrado).

---

## Estado

**Hito 31 cerrado** (`v0.31.0-hito31`, suite **265/5499**). Scope chico — solo los pendientes bajo-coste de los Hitos 29 y 30 (D + F). NavAgent polish quedó descartado permanentemente por decisión del dev ("no me interesa").

## Bloques cerrados

- [x] **Bloque D — Polish char controller** (commit `7dd1133`): friction 0.2→0.5 en createBody + crouch lerp visual de 200ms + headbob sutil 5Hz/4cm. Aplicado simétrico en Editor Play Mode y MoodPlayer.
- [x] **Bloque F — Polish particles** (commit `d6aa6af`): flag `localSpace` (default false) + sort back-to-front en alpha blend. Persistencia retro-compat.
- [x] **Cierre — tests + docs + tag**: 1 test nuevo (round-trip de localSpace), 2 entradas en DECISIONS.md (crouch lerp + localSpace), tag `v0.31.0-hito31`.

NavAgent polish queda descartado permanentemente por decisión del dev — no proponerlo.

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
