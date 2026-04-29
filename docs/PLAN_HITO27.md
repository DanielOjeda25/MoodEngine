# Plan — Hito 27: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 26 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 26 cerrado** (`v0.26.0-hito26`, suite **212/5326**). Bloques: extracción de texturas embedded/external, Kenney Survival Kit (80 props CC0), IBL bakeable desde equirect, AssetBrowser recursivo, fix UV flip + autoscale bidireccional.

El Hito 27 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

---

## Candidatos

Lista en orden de coste/riesgo creciente. Cualquiera de estos es un hito válido por separado.

### A. Polish del NavAgent (rotación + persistencia) — diferido del Hito 25 y 26

**Por qué:** ahora SÍ tenemos personajes reales (Fox, CesiumMan, varios props de Kenney). El polish del NavAgent es:
1. **Rotación hacia movimiento**: hoy CesiumMan camina mirando siempre en su orientación inicial. Fix corto: `tf.rotationEuler.y = atan2(...)` después del cálculo de `dir`. La fórmula depende del axis "forward" del modelo post-importRotation.
2. **Persistencia del NavAgent en `.moodmap`**: V1 no se persiste; al guardar/cerrar/reabrir el enemigo demo no vuelve. Bump de format version + `SavedNavAgent { speed }`.

**Coste:** bajo. Un par de horas máximo entre código + tests.

**Trigger ideal:** primer nivel real con enemigos persistidos. Está maduro para abrir.

### B. PackageBuilder smart-pack (solo assets referenciados) — identificado en Hito 26

**Por qué:** hoy `PackageBuilder` copia `assets/` entero al package final. Con el Kenney pack (1.5 MB, 80 props), un juego que use 3 props igual ship 80. Para juegos chicos no importa; con 2-3 packs más es problema.

**Coste:** medio. Escanear el `.moodmap` activo + árbol de prefabs + scripts referenciados, recolectar el set de paths efectivamente usados, copiar solo esos. Cuidado con dependencias indirectas (material → textura → png).

**Trigger ideal:** primer demo packageado que pese > 50 MB; o cuando aparezca otro asset pack grande.

### C. Mini editor de scripts in-place (deferido desde Hito 22)

**Por qué:** completar el workflow de scripting Lua del Hito 22. Hoy el dev edita en VS Code y el hot-reload del `ScriptSystem` levanta los cambios — funciona, pero rompe el ciclo "todo en el editor".

**Coste:** bajo-medio. `ImGuiColorTextEdit` es la dep canónica; integrarlo + serializar el contenido + autosave on Lose Focus.

**Trigger ideal:** sesión de scripting intensa donde el ida-y-vuelta a VS Code moleste.

### D. Save/Load de gameplay (HUD + entidades dinámicas)

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego (HP/Ammo del HUD, posición runtime de entidades dinámicas, scripts globals). Save & load sería el primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir qué se persiste (state vs setup), serializar `GameState::hud()` + globals Lua relevantes, integrar con un menú "Save/Load" en el `MoodPlayer`.

**Trigger ideal:** un demo que dependa de progresión (ej. enemigo con HP que sobrevive entre sesiones).

### E. Particle system

**Por qué:** efectos visuales (fuego, humo, sparks) son la única gran feature de render que falta para tener algo visualmente "completo". Forward+ del Hito 18 ya soporta muchas point lights. El hot-reload de shaders del Hito 25 facilita la iteración del shader de particles.

**Coste:** medio-alto. Diseño del sistema (CPU vs GPU, struct of arrays, shading), shader con billboards + soft particles, editor curve para params, persistencia.

**Trigger ideal:** hito de polish visual cuando el sistema base esté estable.

### F. Networking básico (cliente-servidor)

**Por qué:** salto cualitativo grande — habilita multiplayer. Costo grande también.

**Coste:** alto. Capa de transporte (probablemente ENet o yojimbo), serialización de snapshots, predicción/reconciliación, lobby básico.

**Trigger ideal:** se descarta hasta tener al menos save/load + UI de menú principal.

### G. Inspector que permita cambiar la textura de un material — pendiente del Hito 26

**Por qué:** hoy el albedo se setea solo al spawn (vía `createMaterialsForMesh`). Si el dev quiere cambiar la textura de un cubo dropeado, no hay UI — tiene que reasignar el material entero. Drop de textura sobre un material slot del Inspector lo resolvería.

**Coste:** bajo. Reusar la lógica de drag-payload de Asset Browser, ya hay infra para `MOOD_TEXTURE_ASSET`. Aceptar el drop sobre el ImGui::Text de "albedo: %u" del Inspector.

**Trigger ideal:** combo con cualquier hito visual; útil sin ser indispensable.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos arriba se elige.
- [ ] Si el plan toca persistencia, definir si bumpea format version o si usa un campo opcional.
- [ ] Si toca scripting, definir si los nuevos features de Lua respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
