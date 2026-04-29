# Plan — Hito 30: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 29 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 29 cerrado** (`v0.29.0-hito29`, suite **257/5476**). Particle system CPU + billboards instanciados + persistencia + Inspector + spawner demo de fuego.

El Hito 30 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

**Recordatorio importante:** networking / multijugador está **EXPLÍCITAMENTE fuera de alcance** desde el doc técnico (`MOODENGINE_CONTEXTO_TECNICO.md` sección "Fuera de alcance"). NO proponerlo como candidato.

---

## Candidatos activos

Lista en orden de prioridad/coste. Cualquiera de estos es un hito válido por separado.

### A. Player Character Controller con Jolt

**Por qué:** el Hito 12 metió Jolt para rigid bodies pero el JUGADOR sigue usando colisiones AABB del Hito 4. Para un FPS de calidad necesitamos `JPH::CharacterVirtual`: capsule de movimiento con step-up de escalones, slope sliding, jump, crouch. Jolt lo soporta nativo. Es el último gran "agujero" entre el motor actual y "puede ser un FPS de verdad".

**Coste:** medio-alto. Reemplazar la lógica de colisión del jugador en `PlayerApplication`/`EditorPlayMode`. Wire de input (W/A/S/D, Space, Ctrl). Tunear parámetros (capsule height/radius, max slope, jump velocity).

**Trigger ideal:** ahora — sin esto, ningún demo del motor se siente como FPS.

### B. Raycasts + Triggers expuestos a Lua

**Por qué:** Jolt soporta raycasts y body sensors/triggers. Habilitarlos abre dos features grandes de gameplay scriptable:
1. **Raycast desde Lua**: armas, line-of-sight de enemigos, picking físico, interacciones a distancia.
2. **OnTriggerEnter callback**: zonas que detectan entrada sin física (puertas, kill volumes, checkpoints, gatillos).

**Coste:** medio. API en `PhysicsWorld` + bindings en `LuaBindings::physics.raycast(origin, dir, dist)` + `TriggerComponent` con callback `on_enter`/`on_exit`.

**Trigger ideal:** combo con A (Player Controller), o cuando el dev empiece a construir gameplay scriptable.

### C. Save/Load de gameplay state

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego (HP/Ammo del HUD, posición runtime de entidades dinámicas, scripts globals). Save & load sería el primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### D. Polish del NavAgent (rotación + persistencia)

**Por qué:** pendiente diferido del Hito 23 + Hito 25 + Hito 26 + Hito 27. Hoy CesiumMan camina sin girar, y el NavAgent no se persiste en `.moodmap`. Dos fixes pequeños:
1. `NavSystem::update`: setear `tf.rotationEuler.y = atan2(...)` después del cálculo de `dir`.
2. Extender `SavedEntity` con `nav_agent: { speed, active }`.

**Coste:** bajo. Un par de horas máximo.

**Trigger ideal:** combo con A (player controller) si hace falta probar el NavAgent contra un FPS de verdad.

### E. Sorting de partículas + local space (pendientes del Hito 29)

**Por qué:** dos pendientes menores explícitos del Hito 29. Sort back-to-front por depth elimina artifacts de blending entre emisores superpuestos. Local space attached al emisor permite humo siguiendo personajes en movimiento.

**Coste:** bajo (ambos). Se pueden hacer juntos.

**Trigger ideal:** combo de polish si A/B/C bloquean.

### F. UI de juego mejorada (HUD + main menu)

**Por qué:** el Hito 20 entregó HUD básico (HP/Ammo) + menú de pausa. Falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos (resolución, audio, sensibilidad), y HUD parametrizable desde Lua/Inspector.

**Coste:** medio.

**Trigger ideal:** combo con C (save/load) — sin eso "Load Game" no tiene qué cargar.

---

## Diferidos (revisar más adelante)

### InspectorEditCommand (genérico) — del Hito 28

51 widgets en 511 líneas requieren un patrón templado. **Trigger:** dev pide undo de sliders del Inspector.

### Commands para drops modificadores — del Hito 28

Texture / material / script drops no pasan por history. **Trigger:** combo con InspectorEditCommand.

### Handle remap en HistoryStack — del Hito 27

edit→delete→undo→undo deja segundo undo no-op silencioso. **Trigger:** dev nota el comportamiento.

### Sorting / local space para partículas — del Hito 29

Ver candidato E si se elige ese path.

### PackageBuilder smart-pack — del Hito 26

Solo copiar assets referenciados por el .moodmap activo. **Trigger:** primer demo packageado > 50 MB.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos features de Lua respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
