# Plan — Hito 32: Cerrar deudas del editor (cerrado)

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 32 cerrado).

---

## Estado

**Hito 32 cerrado** (`v0.32.0-hito32`, suite **271/5512**). Scope acordado con el dev: limpiar deudas del editor antes de abordar features pesados (raycasts, save/load, UI menu).

## Bloques cerrados

- [x] **Bloque D — InspectorEditCommand** (commit pendiente): templated `EditPropertyCommand<T>` + helper `trackPropertyEdit<T>` con detección drag-end. 9 widgets cableados (Tag, Transform pos/rot/scale, Material albedoTint/metallic/roughness/ao). Compactación automática vía `IsItemDeactivatedAfterEdit` (un drag = un comando).
- [x] **Bloque "Handle remap"**: `ICommand::onEntityRemap` virtual + `HistoryStack::remapEntityInStack`. `DeleteEntityCommand` dispara post-undo. `EditTransform`, `EditProperty`, `CreateEntity` overridan. Flujo `edit→delete→undo→undo` completa correctamente.
- [x] **Cierre — tests + docs + tag**: 6 tests nuevos en `test_edit_property_command`, 2 entradas en DECISIONS.md, tag `v0.32.0-hito32`.

## Lo que NO se cubrió

- **42 widgets restantes del Inspector** (Light, Camera, AudioSource, ParticleEmitter, Animator, NavAgent, Environment, RigidBody): patrón uniforme — wire-up mecánico siguiendo el `pushEditIfDone<T>` ya cableado. Se puede hacer per-widget según se necesite.
- **Inspector drop de textura sobre material slot**: era candidato pero quedó fuera por scope.
- **PackageBuilder smart-pack**: candidato medio que quedó para Hito 33+.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Raycasts + Triggers expuestos a Lua

**Por qué:** combo natural post-Hito 30+31. Jolt soporta raycasts y body sensors/triggers. Habilitarlos abre las dos features grandes pendientes para gameplay scriptable:
1. **Raycast desde Lua**: armas (line-of-sight + hitscan), enemigos que ven al jugador, picking físico, interacciones a distancia. `physics.raycast(origin, dir, maxDist) → (hit, point, normal, entity)`.
2. **OnTriggerEnter/Exit**: zonas que detectan entrada sin colisión sólida (puertas automáticas, kill volumes, checkpoints, gatillos de scripts). `TriggerComponent { halfExtents }` + callback Lua `on_enter(other)` / `on_exit(other)`.

**Coste:** medio. API en `PhysicsWorld` + bindings en `LuaBindings::physics` + `TriggerComponent` con dispatch al ScriptSystem.

**Trigger ideal:** ahora — el char controller pulido del Hito 31 + raycasts/triggers = gameplay scriptable real (FPS con armas + zonas).

### B. Save/Load de gameplay state

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego. Save & load = primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### C. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con B (sin save/load no hay "Load Game").

### D. Completar Undo/Redo: InspectorEditCommand — pendiente de Hito 27 + 28

**Por qué:** hoy `albedoTint` / `metallic` / `roughness` / `ao` / valores del transform desde el Inspector NO pasan por el history. Ctrl+Z no los revierte. 51 widgets en 511 líneas requieren un patrón templado (`PropertyEditCommand<T>` con `IsItemActivated`/`IsItemDeactivatedAfterEdit`).

**Coste:** medio. Patrón uniforme (template) + tail por widget.

**Trigger ideal:** dev pide undo de sliders.

---

## Diferidos (revisar más adelante)

### PackageBuilder smart-pack — del Hito 26

Solo copiar assets referenciados. **Trigger:** demo packageado > 50 MB.

### Inspector que permita cambiar la textura de un material via drop — del Hito 26

Coste bajo. **Trigger:** combo con cualquier hito visual.

### Handle remap en HistoryStack — del Hito 27

edit→delete→undo→undo deja segundo undo no-op silencioso. **Trigger:** dev nota el comportamiento.

### Compactación de comandos (sliders del Inspector) — del Hito 27

Mover un slider 30 frames produce 30 entradas en el history. **Trigger:** combo con D (InspectorEditCommand).

### Polish menores del Hito 31

- Sort por bucket-Z (vs centro de partícula simple) si aparecen artifacts visibles.
- `localSpace` con rotation/scale del entity (sparks orientadas con un personaje).
- Headbob escalado con velocidad.
- Friction per-body en `RigidBodyComponent`.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
