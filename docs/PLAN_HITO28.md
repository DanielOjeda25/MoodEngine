# Plan — Hito 28: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 27 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 27 cerrado** (`v0.27.0-hito27`, suite **238/5409**). Bloques: HistoryStack + ICommand, EditTransformCommand (gizmo), DeleteEntityCommand (snapshot via EntitySerializer), hotkeys Ctrl+Z/Y, menú Editar reactivo, 26 tests nuevos.

El Hito 28 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

**Recordatorio importante:** networking / multijugador está **EXPLÍCITAMENTE fuera de alcance** desde el doc técnico (`MOODENGINE_CONTEXTO_TECNICO.md` sección "Fuera de alcance"). NO proponerlo como candidato.

---

## Candidatos activos

Lista en orden de coste/riesgo creciente.

### A. Completar Undo/Redo: CreateEntityCommand + edits del Inspector — pendiente del Hito 27

**Por qué:** el Hito 27 cerró Ctrl+Z para gizmo + delete, pero hay 2 buracos visibles:
1. **Spawn/drop undoable**: hoy un Ctrl+Z después de "Ayuda > Spawn enemigo" o un drop de mesh al viewport NO destruye la entidad recién creada. Asimétrico con delete (que sí es undoable).
2. **Inspector edits**: cambiar `albedoTint` / `metallic` / `roughness` / `ao` / valores del transform desde el Inspector NO pasa por el history. Ctrl+Z no los revierte.

**Coste:** medio. Hay ~10 spawn paths (`processSpawnRotator`, `processSpawnEnemyDemo`, etc.) que hay que wrappear; el Inspector tiene varios sliders por cada componente. Patrón ya existe (DeleteEntityCommand sirve de guía para el snapshot-based approach).

**Trigger ideal:** ahora — el dev sufre la asimetría cada vez que prueba Ctrl+Z después de spawnear.

### B. Player Character Controller con Jolt — preparación para FPS real

**Por qué:** el Hito 12 metió Jolt para rigid bodies pero el JUGADOR sigue usando colisiones AABB del Hito 4. Para un FPS de calidad necesitamos `JPH::CharacterVirtual`: capsule de movimiento con step-up de escalones, slope sliding, jump, crouch. Jolt lo soporta nativo.

**Coste:** medio-alto. Reemplazar la lógica de colisión del jugador en `PlayerApplication`/`EditorPlayMode`. Wire de input (W/A/S/D, Space, Ctrl). Tunear parámetros (capsule height/radius, max slope, jump velocity).

**Trigger ideal:** primer demo donde se quiera que el jugador suba escaleras o salte plataformas.

### C. Raycasts + Triggers expuestos a Lua

**Por qué:** Jolt soporta raycasts y body triggers/sensors. Habilitarlos abre dos features de gameplay grandes:
1. **Raycast desde Lua**: armas, line-of-sight de enemigos, picking físico.
2. **OnTriggerEnter callback**: zonas que detectan entrada sin física (puertas automáticas, kill volumes, checkpoints, gatillos).

**Coste:** medio. API en `PhysicsWorld` + bindings en `LuaBindings::physics.raycast(origin, dir, dist)` + nuevo `TriggerComponent` con callback `on_enter`/`on_exit`.

**Trigger ideal:** combo con B (Player Controller), o cuando el dev empiece a construir gameplay scriptable.

### D. Save/Load de gameplay (HUD + entidades dinámicas) — diferido del Hito 27

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego. Save & load sería el primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** un demo que dependa de progresión.

### E. Particle system

**Por qué:** efectos visuales (fuego, humo, sparks) son la única gran feature de render que falta para tener algo visualmente "completo". El hot-reload shaders del Hito 25 facilita la iteración.

**Coste:** medio-alto. Diseño del sistema (CPU vs GPU), shader con billboards + soft particles, editor curve para params, persistencia.

**Trigger ideal:** hito de polish visual.

### F. Mini editor de scripts in-place — diferido desde Hito 22

**Por qué:** completar el workflow de scripting Lua. Hoy el dev edita en VS Code y el hot-reload del `ScriptSystem` levanta los cambios — funciona, pero rompe el ciclo "todo en el editor".

**Coste:** bajo-medio. `ImGuiColorTextEdit` es la dep canónica.

**Trigger ideal:** sesión de scripting intensa.

### G. Fix del autoscale agresivo de Kenney — TODO del Hito 26

**Por qué:** meshes chicos (Kenney barriles ~0.27m) reciben scale ~5.4x para llegar a ~1.5m. Visualmente "gruesos" comparados al world.

**Coste:** bajo. Mejor approach: metadata por asset pack (cm/m/inch) o slider "scale al drop" en el viewport.

**Trigger ideal:** combo con A (commands para spawn) — también encaja con un hito de polish editor.

---

## Diferidos (revisar más adelante)

### Polish del NavAgent (rotación + persistencia)

Diferido del Hito 25, 26 y 27. Coste bajo. Trigger: primer nivel real con enemigos persistidos.

### PackageBuilder smart-pack (solo assets referenciados)

Identificado en Hito 26. Coste medio. Trigger: primer demo packageado > 50 MB.

### Inspector que permita cambiar la textura de un material via drop

Pendiente del Hito 26. Coste bajo. Trigger: combo con cualquier hito visual.

### Handle remap en HistoryStack tras delete-undo

Identificado en Hito 27 como pendiente menor. Coste bajo. Trigger: dev nota el comportamiento al hacer edit→delete→undo→undo.

### Compactación de comandos (sliders del Inspector)

Identificado en Hito 27. Mover un slider 30 frames produce 30 entradas en el history. Coste bajo si el patrón se diseña una vez.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si el plan toca persistencia, definir si bumpea format version o si usa un campo opcional.
- [ ] Si toca scripting, definir si los nuevos features de Lua respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
