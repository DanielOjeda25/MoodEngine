# Plan — Hito 36: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 35 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 35 cerrado** (`v0.35.0-hito35`, suite **287/5561**). 4 hitos seguidos cerrando deudas (32 → 33 → 34 → 35). Inspector con undo en 40 widgets. Bug latente del Hito 26 con paths `..` resuelto. Drop de textura sobre material slot habilitado.

El Hito 36 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

**Recordatorios importantes:**
- Networking / multijugador, i18n, cursores custom **fuera de alcance**. NO proponer.
- Polish del NavAgent **descartado permanentemente**.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Save/Load de gameplay state

**Por qué:** hoy `.moodmap` describe nivel pero NO estado de juego. Save & load = primer eslabón de "juego con progresión". Muchos hitos cerrando deudas — el dev probablemente esté listo para una feature visible.

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### B. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con A.

### C. PackageBuilder smart-pack — del Hito 26

**Por qué:** hoy `PackageBuilder` copia `assets/` entero. Con Kenney pack (1.5 MB) + futuros packs, los packages se inflan rápido.

**Coste:** medio. Escanear `.moodmap` + dependencias indirectas.

**Trigger ideal:** demo packageado > 50 MB.

### D. Triggers detectan dynamic bodies / NPCs — del Hito 33

**Por qué:** hoy solo el char del jugador es el "actor" detectable. Cajas físicas o NPCs entrando/saliendo no disparan callback.

**Coste:** medio. Loop adicional sobre `RigidBodyComponent`.

**Trigger ideal:** primer demo que requiera detectar otra cosa (puzzles tipo "pesar un objeto para abrir puerta", proyectiles atravesando zona).

### E. Emisión de partículas por shape — del Hito 29

**Por qué:** hoy todas las partículas se emiten desde un punto en el centro del Transform. Cono/esfera/disco amplían expresividad de VFX.

**Coste:** medio. Refactor del emit step + nuevo enum + sample funcs por shape.

**Trigger ideal:** VFX que pide directionalidad clara (chispas orientadas, humo cónico).

### F. Undo del drop de textura sobre material slot — del Hito 35 A

**Por qué:** drop textura cambia el MaterialAssetId del slot, pero no se trackea en el HistoryStack.

**Coste:** bajo. Comando dedicado `ReplaceMaterialCommand`.

**Trigger ideal:** dev nota la falta tras tweakear texturas.

---

## Diferidos (revisar más adelante)

### Polish menores acumulados
- Sort de partículas por bucket-Z (vs centro simple).
- `localSpace` con rotation/scale del entity.
- `std::variant` del tracker no incluye `int`/`u32` (necesario para `DragInt` como maxParticles).
- Compactación cross-frame para sliders externos al Inspector.
- Trigger AABB no respeta rotation del Transform.
- Raycast batch API + filtros por layer/tag genérico.
- Setter runtime de friction (hoy aplica solo al re-materializar).
- Coyote/jump buffer windows configurables per-proyecto.
- ~10 widgets estructurales del Inspector sin undo.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox.

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
