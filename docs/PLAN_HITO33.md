# Plan — Hito 33: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 32 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 32 cerrado** (`v0.32.0-hito32`, suite **271/5512**). Deudas del editor cerradas: InspectorEditCommand templado + handle remap en HistoryStack.

El Hito 33 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

**Recordatorios importantes:**
- Networking / multijugador está **EXPLÍCITAMENTE fuera de alcance** desde el doc técnico. NO proponerlo.
- Polish del NavAgent **descartado permanentemente** por decisión del dev. NO proponerlo.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Raycasts + Triggers expuestos a Lua

**Por qué:** combo natural post-Hitos 30/31/32. Char controller pulido + raycasts + triggers = FPS scriptable real. Habilita:
1. **Raycast desde Lua**: armas (line-of-sight + hitscan), enemigos viendo al jugador, picking físico.
2. **OnTriggerEnter/Exit**: zonas detectoras sin física sólida (puertas automáticas, kill volumes, checkpoints).

**Coste:** medio. API en `PhysicsWorld` + bindings en `LuaBindings::physics` + nuevo `TriggerComponent` con dispatch al ScriptSystem.

**Trigger ideal:** ahora — es el siguiente paso obvio para gameplay scriptable.

### B. Save/Load de gameplay state

**Por qué:** hoy `.moodmap` describe nivel pero NO estado de juego. Save & load = primer eslabón de "juego con progresión".

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### C. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con B (sin save/load no hay "Load Game").

### D. Wire-up del Inspector restante (42 widgets) — del Hito 32

**Por qué:** Hito 32 cableó 9 de 51 widgets con InspectorEditCommand. Los otros 42 son mecánicos (mismo patrón `pushEditIfDone<T>`). Cubre Light, Camera, AudioSource, ParticleEmitter, Animator, NavAgent, Environment, RigidBody.

**Coste:** bajo (mecánico) pero tedioso. Mejor hacer en bloque que widget-por-widget.

**Trigger ideal:** sesión donde el dev quiere dejar el Inspector "completo" en undo.

### E. PackageBuilder smart-pack — del Hito 26

**Por qué:** hoy `PackageBuilder` copia `assets/` entero. Con Kenney pack (1.5 MB) + futuros packs, los packages se inflan rápido.

**Coste:** medio. Escanear `.moodmap` + dependencias indirectas (material → textura).

**Trigger ideal:** demo packageado > 50 MB.

### F. Inspector drop de textura sobre material slot — del Hito 26

**Por qué:** hoy el albedo se setea solo al spawn. Drop de textura sobre el slot del Inspector permitiría cambiar texturas sin reasignar el material entero.

**Coste:** bajo. Reusar drag payload `MOOD_TEXTURE_ASSET`.

**Trigger ideal:** combo con cualquier hito visual.

---

## Diferidos (revisar más adelante)

### Polish menores del Hito 31

- Sort por bucket-Z (vs centro de partícula simple).
- `localSpace` con rotation/scale del entity (sparks orientadas).
- Headbob escalado con velocidad.
- Friction per-body en `RigidBodyComponent`.

### Polish menores del Hito 32

- `std::variant` del tracker no incluye `int`/`u32` (necesario para `DragInt`).
- Compactación cross-frame para sliders externos al Inspector.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
