# Plan — Hito 26: TBD

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 25 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`. Decisiones nuevas van acá y en `DECISIONS.md`.

---

## Estado

**Hito 25 cerrado** (`v0.25.0-hito25`, suite **206/5309**). Bloques: hot-reload de shaders + polish del sistema de materiales (default visible como warning, instance único por entidad, costura del skybox).

El Hito 26 está **TBD**: acordar con el dev el alcance antes de abrir bloques.

---

## Candidatos

Lista en orden de coste/riesgo creciente. Cualquiera de estos es un hito válido por separado.

### A. Polish del NavAgent (rotación + persistencia) — deferido del Hito 25

**Por qué:**
1. Rotación hacia movimiento: hoy CesiumMan camina mirando siempre en su orientación inicial. Fix corto: `tf.rotationEuler.y = atan2(...)` después del cálculo de `dir`. La fórmula depende del axis "forward" del modelo post-importRotation.
2. Persistencia del NavAgent en `.moodmap`: V1 no se persiste; al guardar/cerrar/reabrir el enemigo demo no vuelve. Bump de format version + `SavedNavAgent { speed }`.

**Coste:** bajo. Un par de horas máximo entre código + tests.

**Trigger ideal:** un dev quiere armar un nivel con enemigos persistidos (no spawn cada vez via menú "Ayuda"). El dev mencionó que "es problema para más adelante cuando ya tenga personajes de verdad" — abre cuando aparezca el primer personaje real.

### B. Mini editor de scripts in-place (deferido desde Hito 22)

**Por qué:** completar el workflow de scripting Lua del Hito 22. Hoy el dev edita en VS Code y el hot-reload del `ScriptSystem` levanta los cambios — funciona, pero rompe el ciclo "todo en el editor". Stretch original del Hito 22.

**Coste:** bajo-medio. `ImGuiColorTextEdit` es la dep canónica; integrarlo + serializar el contenido + autosave on Lose Focus.

**Trigger ideal:** las exposed properties del Hito 24 ya hacen los scripts más portables; con el hot-reload de shaders del Hito 25 se cerró el otro frente de iteración. El siguiente paso natural es editar todo desde el editor.

### C. Save/Load de gameplay (HUD + entidades dinámicas)

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego (HP/Ammo del HUD, posición runtime de entidades dinámicas, scripts globals). Save & load sería el primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir qué se persiste (state vs setup), serializar `GameState::hud()` + globals Lua relevantes, integrar con un menú "Save/Load" en el `MoodPlayer`.

**Trigger ideal:** un demo del Hito 25 que dependa de progresión (ej. enemigo con HP que sobrevive entre sesiones).

### D. Networking básico (cliente-servidor)

**Por qué:** salto cualitativo grande — habilita multiplayer. Costo grande también.

**Coste:** alto. Capa de transporte (probablemente ENet o yojimbo), serialización de snapshots, predicción/reconciliación, lobby básico.

**Trigger ideal:** se descarta hasta tener al menos save/load + UI de menú principal.

### E. Asset pack realista (Quaternius / Polyhaven / Mixamo)

**Por qué:** mencionado por el dev en Hito 23. Hoy los demos usan primitivas + pyramid/Fox/CesiumMan. Tener 3-5 props (cajas, barriles, mobiliario CC0) y 1-2 personajes humanoides amplía qué se puede mostrar.

**Coste:** bajo (selección + commit de assets + un demo con scripts) si se ciñe al pipeline actual. Medio si hay que bake nuevos materiales/normales.

**Trigger ideal:** hito de polish — buen complemento si A o B se cierra rápido.

### F. Particle system

**Por qué:** efectos visuales (fuego, humo, sparks) son la única gran feature de render que falta para tener algo visualmente "completo". Forward+ del Hito 18 ya soporta muchas point lights — añadir particles encajaría. Bonus: el hot-reload de shaders del Hito 25 facilita la iteración del shader de particles.

**Coste:** medio-alto. Diseño del sistema (CPU vs GPU, struct of arrays, shading), shader con billboards + soft particles, editor curve para params, persistencia.

**Trigger ideal:** hito de polish visual cuando el sistema base esté estable.

### G. Sync de IBL al skybox equirect

**Por qué:** pendiente menor del Hito 24. Hoy `SceneRenderer` usa el equirect kloofendal pero el IBL bake (`tools/bake_ibl.py`) sigue usando `assets/skyboxes/sky_day/` cubemap — la iluminación PBR no refleja el cielo visible. Visualmente confuso en escenas con esferas metálicas.

**Coste:** bajo. Adaptar `bake_ibl.py` para aceptar equirect como input + regenerar IBL maps + actualizar `IBL` loader del SceneRenderer si hace falta.

**Trigger ideal:** combo con E (asset pack) o F (particles) para un hito de polish visual.

### H. PackageBuilder smart-pack (solo assets referenciados)

**Por qué:** identificado durante el Hito 26 al sumar el Kenney Survival Kit (1.5 MB con 80 props). Hoy `PackageBuilder` copia `assets/` entero al package final del juego — si el `.moodmap` solo usa 3 de los 80 props, los 77 restantes igual viajan. Para juegos chicos no importa; para uno con varios asset packs (Polyhaven, Quaternius, etc.) el package se infla rápido.

**Coste:** medio. Escanear el `.moodmap` activo + el árbol de prefabs + scripts referenciados, recolectar el set de paths efectivamente usados, copiar solo esos al package destino. Cuidado con dependencias indirectas (un material referencia una textura, una textura referencia un .png, etc.).

**Trigger ideal:** primer juego con > 5 MB de assets sin usar; o cuando se publique algún demo y el package final pese > 50 MB.

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos arriba se elige.
- [ ] Si el plan toca persistencia, definir si bumpea format version o si usa un campo opcional.
- [ ] Si toca scripting, definir si los nuevos features de Lua respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
