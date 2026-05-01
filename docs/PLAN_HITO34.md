# Plan — Hito 34: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 33 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 33 cerrado** (`v0.33.0-hito33`, suite **281/5535**). Raycasts + triggers expuestos a Lua. FPS scriptable real con char controller pulido (Hito 30/31), undo del Inspector (Hito 32), y zonas/armas hitscan (Hito 33).

El Hito 34 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

**Recordatorios importantes:**
- Networking / multijugador está **EXPLÍCITAMENTE fuera de alcance** desde el doc técnico. NO proponerlo.
- Polish del NavAgent **descartado permanentemente** por decisión del dev. NO proponerlo.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Save/Load de gameplay state

**Por qué:** hoy `.moodmap` describe nivel pero NO estado de juego. Save & load = primer eslabón de "juego con progresión". Encaja como continuación natural post Hito 33: ya hay armas (raycast) + zonas (triggers) que pueden cambiar el estado del juego, falta poder guardarlo.

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes (`engine.exposed` overrides ya persisten; agregar runtime values?), integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### B. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con A (sin save/load no hay "Load Game").

### C. Wire-up del Inspector restante (42 widgets) — del Hito 32

**Por qué:** Hito 32 cableó 9 de 51 widgets con InspectorEditCommand. Los otros 42 son mecánicos (mismo patrón `pushEditIfDone<T>`). Cubre Light, Camera, AudioSource, ParticleEmitter, Animator, NavAgent, Environment, RigidBody, Trigger (que ya tiene 1 cableado del Hito 33 — los demás del componente serían `playerInside` que NO debería ser editable manualmente).

**Coste:** bajo (mecánico) pero tedioso. Mejor hacer en bloque que widget-por-widget.

**Trigger ideal:** sesión donde el dev quiere dejar el Inspector "completo" en undo.

### D. PackageBuilder smart-pack — del Hito 26

**Por qué:** hoy `PackageBuilder` copia `assets/` entero. Con Kenney pack (1.5 MB) + futuros packs, los packages se inflan rápido.

**Coste:** medio. Escanear `.moodmap` + dependencias indirectas (material → textura; script → exposed property → asset path).

**Trigger ideal:** demo packageado > 50 MB.

### E. Inspector drop de textura sobre material slot — del Hito 26

**Por qué:** hoy el albedo se setea solo al spawn. Drop de textura sobre el slot del Inspector permitiría cambiar texturas sin reasignar el material entero.

**Coste:** bajo. Reusar drag payload `MOOD_TEXTURE_ASSET`.

**Trigger ideal:** combo con cualquier hito visual.

### F. Polish reactivo del Hito 33

Si el dev usa los triggers/raycasts en demos reales y aparecen rough edges:
- **Raycast con filtro de layer/tag**: parámetro opcional `ignoredBodyId` o `layerMask`.
- **Triggers con `on_trigger_stay`**: callback per-frame mientras el actor está dentro.
- **Triggers detectan dynamic bodies**: extender el loop a entidades con RigidBody.

**Coste:** bajo cada uno. **Trigger ideal:** demo del dev pide explícitamente la feature.

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

### Polish menores del Hito 33
- Trigger AABB no respeta rotation del Transform (aceptado pero documentado).
- Raycast no expone batch API (`raycastAll`).

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
