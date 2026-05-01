# Plan — Hito 37: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 36 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 36 cerrado** (`v0.36.0-hito36`, suite **291/5574**). 5 hitos seguidos cerrando deudas (32 → 33 → 34 → 35 → 36). El backlog "scope chico arrastrando" está limpio. Inspector con undo en 41 widgets totales. Triggers con enter/exit/stay completos. Drop textura material undoable.

El Hito 37 está **TBD**: acordar con el dev el alcance. Tras 5 hitos de cleanup, el dev probablemente esté listo para feature pesada visible.

**Recordatorios importantes:**
- Networking / multijugador, i18n, cursores custom **fuera de alcance**. NO proponer.
- Polish del NavAgent **descartado permanentemente**.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Save/Load de gameplay state

**Por qué:** hoy `.moodmap` describe nivel pero NO estado de juego. Save & load = primer eslabón de "juego con progresión". Tras 5 hitos cerrando deudas, el dev probablemente está listo para una feature visible. Habilita pasar el primer demo "real" con progresión.

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### B. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con A.

### C. PackageBuilder smart-pack — del Hito 26

**Por qué:** hoy `PackageBuilder` copia `assets/` entero. Con Kenney pack (1.5 MB) + futuros packs, los packages se inflan rápido.

**Coste:** medio.

**Trigger ideal:** demo packageado > 50 MB.

### D. Triggers detectan dynamic bodies / NPCs — del Hito 33

**Por qué:** hoy solo el char del jugador es el "actor" detectable. Cajas físicas o NPCs entrando/saliendo no disparan callback.

**Coste:** medio.

**Trigger ideal:** primer demo que requiera detectar otra cosa (puzzles tipo "pesar un objeto para abrir puerta").

### E. Emisión de partículas por shape — del Hito 29

**Por qué:** hoy todas las partículas se emiten desde un punto en el centro del Transform. Cono/esfera/disco amplían expresividad de VFX.

**Coste:** medio.

**Trigger ideal:** VFX que pide directionalidad clara.

---

## Diferidos (revisar más adelante)

### Polish menores acumulados (sin trigger concreto hoy)
- Sort de partículas por bucket-Z (vs centro simple).
- `localSpace` con rotation/scale del entity.
- Compactación cross-frame para sliders externos al Inspector.
- Trigger AABB no respeta rotation del Transform.
- Raycast batch API + filtros por layer/tag genérico.
- Setter runtime de friction (hoy aplica solo al re-materializar).
- Coyote/jump buffer windows configurables per-proyecto.
- DragFloatRange2 widgets undoables (lifetime/size).
- Combos estructurales del Inspector (type/shape/clipName).

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox.

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
