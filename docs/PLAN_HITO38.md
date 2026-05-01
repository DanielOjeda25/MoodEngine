# Plan — Hito 38: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 37 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 37 cerrado** (`v0.37.0-hito37`, suite **300/5927**). 6 hitos seguidos cerrando deudas (32 → 33 → 34 → 35 → 36 → 37). El backlog de scope chico-medio arrastrando está limpio: undo del Inspector completo (40+ widgets), triggers con enter/exit/stay para player + dynamic bodies, raycast filtrable, friction per-body, coyote+jump-buffer, headbob velocity scale, drop textura material undoable, smart-pack del PackageBuilder, particle emission shapes.

El Hito 38 está **TBD**: el dev probablemente está listo para feature visible nueva.

**Recordatorios importantes:**
- Networking / multijugador, i18n, cursores custom **fuera de alcance**. NO proponer.
- Polish del NavAgent **descartado permanentemente**.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Save/Load de gameplay state

**Por qué:** hoy `.moodmap` describe nivel pero NO estado de juego. Save & load = primer eslabón de "juego con progresión". Tras 6 hitos cerrando deudas, es la primera feature visible que abre demos nuevos.

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo con progresión.

### B. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con A.

### C. Polish reactivo del Hito 37

Si el dev usa los nuevos features y aparecen rough edges:
- Trigger AABB con rotation (OBB).
- Particle emission shape `Cone` (con dirección).
- Setter runtime de friction (hoy aplica solo al re-Play).
- Filtro raycast layer/tag genérico.

**Coste:** bajo cada uno.

**Trigger ideal:** demo del dev pide explícitamente la feature.

---

## Diferidos (revisar más adelante)

### Polish menores acumulados (sin trigger concreto hoy)
- Sort de partículas por bucket-Z (vs centro simple).
- `localSpace` con rotation/scale del entity.
- Compactación cross-frame para sliders externos al Inspector.
- Coyote/jump buffer windows configurables per-proyecto.
- DragFloatRange2 widgets undoables (lifetime/size del ParticleEmitter — requiere `pair<f32,f32>` en variant).
- Combos estructurales del Inspector (type/shape/clipName).

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox.

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
