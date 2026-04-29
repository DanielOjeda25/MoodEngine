# Plan — Hito 27: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 26 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 26 cerrado** (`v0.26.0-hito26`, suite **212/5326**). Bloques: extracción de texturas embedded/external, Kenney Survival Kit (80 props CC0), IBL bakeable desde equirect, AssetBrowser recursivo, fix UV flip + autoscale bidireccional.

El Hito 27 está **TBD**: acordar con el dev el alcance antes de abrir bloques. Por decisión del dev al cerrar el Hito 26, los candidatos NavAgent polish, PackageBuilder smart-pack, e Inspector con drop de textura quedaron fuera del menu — anotados en la sección "Diferidos" abajo para revisión futura.

---

## Candidatos activos

Lista en orden de coste/riesgo creciente. Cualquiera de estos es un hito válido por separado.

### A. Mini editor de scripts in-place (deferido desde Hito 22)

**Por qué:** completar el workflow de scripting Lua del Hito 22. Hoy el dev edita en VS Code y el hot-reload del `ScriptSystem` levanta los cambios — funciona, pero rompe el ciclo "todo en el editor".

**Coste:** bajo-medio. `ImGuiColorTextEdit` es la dep canónica; integrarlo + serializar el contenido + autosave on Lose Focus.

**Trigger ideal:** sesión de scripting intensa donde el ida-y-vuelta a VS Code moleste.

### B. Save/Load de gameplay (HUD + entidades dinámicas)

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego (HP/Ammo del HUD, posición runtime de entidades dinámicas, scripts globals). Save & load sería el primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir qué se persiste (state vs setup), serializar `GameState::hud()` + globals Lua relevantes, integrar con un menú "Save/Load" en el `MoodPlayer`.

**Trigger ideal:** un demo que dependa de progresión (ej. enemigo con HP que sobrevive entre sesiones).

### C. Particle system

**Por qué:** efectos visuales (fuego, humo, sparks) son la única gran feature de render que falta para tener algo visualmente "completo". Forward+ del Hito 18 ya soporta muchas point lights. El hot-reload de shaders del Hito 25 facilita la iteración del shader de particles.

**Coste:** medio-alto. Diseño del sistema (CPU vs GPU, struct of arrays, shading), shader con billboards + soft particles, editor curve para params, persistencia.

**Trigger ideal:** hito de polish visual cuando el sistema base esté estable.

### D. Networking básico (cliente-servidor)

**Por qué:** salto cualitativo grande — habilita multiplayer. Costo grande también.

**Coste:** alto. Capa de transporte (probablemente ENet o yojimbo), serialización de snapshots, predicción/reconciliación, lobby básico.

**Trigger ideal:** se descarta hasta tener al menos save/load + UI de menú principal.

---

## Diferidos (no van al Hito 27, revisar más adelante)

El dev decidió al cerrar el Hito 26 dejar estos fuera del próximo hito. Mantenidos acá para no perder la idea.

### Polish del NavAgent (rotación + persistencia)

Diferido del Hito 25 y 26. Rotación hacia movimiento (CesiumMan camina mirando siempre adelante; `tf.rotationEuler.y = atan2(...)`) + persistir `NavAgentComponent` en `.moodmap` (bump format version + `SavedNavAgent { speed }`). Coste bajo. Trigger: primer nivel real con enemigos persistidos.

### PackageBuilder smart-pack (solo assets referenciados)

Identificado en Hito 26 al sumar 1.5 MB de Kenney. Hoy `PackageBuilder` copia `assets/` entero al package final. Coste medio (escanear `.moodmap` + prefabs + scripts referenciados, recolectar dependencias indirectas material→textura→png). Trigger: primer demo packageado > 50 MB.

### Inspector que permita cambiar la textura de un material

Pendiente del Hito 26. Hoy el albedo se setea solo al spawn vía `createMaterialsForMesh`. Drop de textura sobre el slot albedo del Inspector lo resolvería. Coste bajo (reusar `MOOD_TEXTURE_ASSET` del Asset Browser). Trigger: combo con cualquier hito visual.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos activos arriba se elige.
- [ ] Si el plan toca persistencia, definir si bumpea format version o si usa un campo opcional.
- [ ] Si toca scripting, definir si los nuevos features de Lua respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
