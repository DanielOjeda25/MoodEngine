# Plan — Hito 35: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 34 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 34 cerrado** (`v0.34.0-hito34`, suite **286/5555**). Deudas chicas barridas: friction per-body, raycast filtrable, coyote+jump-buffer, headbob velocity scale, undo de 7 widgets más del Inspector.

El Hito 35 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

**Recordatorios importantes:**
- Networking / multijugador, i18n, cursores custom **fuera de alcance**. NO proponer.
- Polish del NavAgent **descartado permanentemente**.

---

## Candidatos activos

Lista en orden de prioridad/coste.

### A. Save/Load de gameplay state

**Por qué:** hoy `.moodmap` describe nivel pero NO estado de juego. Save & load = primer eslabón de "juego con progresión". Encaja como continuación natural post Hito 33-34: ya hay armas (raycast filtrable), zonas (triggers), feeling de FPS pulido (coyote, headbob).

**Coste:** medio. Definir state vs setup, serializar `GameState::hud()` + globals Lua relevantes, integrar con menú "Save/Load" en `MoodPlayer`.

**Trigger ideal:** primer demo que dependa de progresión.

### B. UI de juego mejorada (HUD + main menu)

**Por qué:** falta menú principal del MoodPlayer (New Game / Load Game / Settings / Quit), settings persistidos, HUD parametrizable.

**Coste:** medio.

**Trigger ideal:** combo con A (sin save/load no hay "Load Game").

### C. PackageBuilder smart-pack — del Hito 26

**Por qué:** hoy `PackageBuilder` copia `assets/` entero. Con Kenney pack (1.5 MB) + futuros packs, los packages se inflan rápido.

**Coste:** medio. Escanear `.moodmap` + dependencias indirectas (material → textura; script → exposed property → asset path).

**Trigger ideal:** demo packageado > 50 MB.

### D. Inspector drop de textura sobre material slot — del Hito 26

**Por qué:** hoy el albedo se setea solo al spawn. Drop de textura sobre el slot del Inspector permitiría cambiar texturas sin reasignar el material entero.

**Coste:** bajo. Reusar drag payload `MOOD_TEXTURE_ASSET`.

**Trigger ideal:** combo con cualquier hito visual.

### E. Triggers detectan dynamic bodies / NPCs — del Hito 33

**Por qué:** hoy solo el char del jugador es el "actor" detectable. Cajas físicas o NPCs entrando/saliendo no disparan callback. Patrón a extender: loop adicional sobre `RigidBodyComponent`.

**Coste:** medio.

**Trigger ideal:** primer demo que requiera detectar otra cosa (puzzles tipo "pesar un objeto para abrir puerta", proyectiles atravesando zona).

### F. Wire-up de los 30+ widgets restantes del Inspector — del Hito 32/34

**Por qué:** Hito 34 cableó 8 widgets más (Light, RigidBody, ParticleEmitter selectos). Quedan ~30 (Camera, AudioSource, Animator, Environment, ParticleEmitter resto). Mecánico.

**Coste:** bajo (mecánico) pero tedioso. Mejor en bloque que widget-por-widget.

**Trigger ideal:** sesión donde el dev quiere dejar el Inspector "completo" en undo.

---

## Diferidos (revisar más adelante)

### Polish menores acumulados
- Sort de partículas por bucket-Z (vs centro simple).
- `localSpace` con rotation/scale del entity (sparks orientadas).
- `std::variant` del tracker no incluye `int`/`u32` (necesario para `DragInt`).
- Trigger AABB no respeta rotation del Transform.
- Raycast batch API (`raycastAll`).
- Setter runtime de friction (hoy aplica solo al re-materializar).
- Coyote/jump buffer windows configurables per-proyecto.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si toca persistencia, definir si bumpea format version o usa campo opcional.
- [ ] Si toca scripting, definir si los nuevos bindings respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
