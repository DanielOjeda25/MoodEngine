# Plan — Hito 25: Hot-reload de shaders + polish del sistema de materiales

> **Leer primero:** `ESTADO_ACTUAL.md`, `DECISIONS.md`, `HITOS.md` (sección Hito 25 cerrado).
>
> **Formato:** cada tarea es un checkbox. Al completar, marcar `[x]`.

---

## Estado

**Hito 25 cerrado** (`v0.25.0-hito25`, suite **206/5309**). Scope acordado con el dev: candidato G (hot-reload de shaders) + polish reactivo del sistema de materiales que aparecieron al testear el render. El candidato B (polish NavAgent) quedó deferido — el dev lo retomará "cuando ya tenga personajes de verdad".

## Bloques cerrados

- [x] **Bloque 1 — Hot-reload de shaders (G)**: `OpenGLShader` con mtime + registry estático + tick global. Recompile atómico con preserve-on-error. Wire-up en `EditorApplication::run()`.
- [x] **Bloque 2 — Polish reactivo de materiales** (cerrado en el mismo tag):
   - Costura del skybox equirect: `texture()` → `textureLod(..., 0.0)`.
   - `MaterialAsset::useAlbedoMap` para distinguir tint puro vs warning visible.
   - `AssetManager::createMaterialFromTexture()` (sin cache) y migración de tiles + spawners + loader. Sentinel `__runtime_tex#<N>` con round-trip por path de textura.
- [x] **Bloque 3 — Tests + docs + tag**: 6 tests nuevos en `test_material_serializer`, smoke test programático del hot-reload, update de `HITOS.md` + `DECISIONS.md` + `ESTADO_ACTUAL.md`, tag `v0.25.0-hito25`, creación de `PLAN_HITO26.md`.

---

## Candidatos

Lista en orden de coste/riesgo creciente. Cualquiera de estos es un hito válido por separado.

### A. Mini editor de scripts in-place (deferido desde Hito 22)

**Por qué:** completar el workflow de scripting Lua del Hito 22. Hoy el dev edita en VS Code y el hot-reload del `ScriptSystem` levanta los cambios — funciona, pero rompe el ciclo "todo en el editor". Stretch original del Hito 22.

**Coste:** bajo-medio. `ImGuiColorTextEdit` es la dep canónica; integrarlo + serializar el contenido + autosave on Lose Focus.

**Trigger ideal:** las exposed properties del Hito 24 ya hacen los scripts más portables — el siguiente paso natural es editarlos sin salir del editor.

### B. Polish del NavAgent (rotación + persistencia) — pendiente del Hito 23

**Por qué:**
1. Rotación hacia movimiento: hoy CesiumMan camina mirando siempre en su orientación inicial. Fix corto: `tf.rotationEuler.y = atan2(...)` después del calculo de `dir`. La fórmula depende del axis "forward" del modelo post-importRotation.
2. Persistencia del NavAgent en `.moodmap`: V1 no se persiste; al guardar/cerrar/reabrir el enemigo demo no vuelve. Bump de format version + `SavedNavAgent { speed }`.

**Coste:** bajo. Un par de horas máximo entre código + tests.

**Trigger ideal:** un dev quiere armar un nivel con enemigos persistidos (no spawn cada vez via menú "Ayuda").

### C. Save/Load de gameplay (HUD + entidades dinámicas)

**Por qué:** hoy el `.moodmap` describe el nivel pero NO el estado de juego (HP/Ammo del HUD, posición runtime de entidades dinámicas, scripts globals). Save & load sería el primer eslabón de "tener un juego con progresión".

**Coste:** medio. Definir qué se persiste (state vs setup), serializar `GameState::hud()` + globals Lua relevantes, integrar con un menú "Save/Load" en el `MoodPlayer`.

**Trigger ideal:** un demo del Hito 24 que dependa de progresión (ej. enemigo con HP que sobrevive entre sesiones).

### D. Networking básico (cliente-servidor)

**Por qué:** salto cualitativo grande — habilita multiplayer. Costo grande tambien.

**Coste:** alto. Capa de transporte (probablemente ENet o yojimbo), serialización de snapshots, predicción/reconciliación, lobby básico.

**Trigger ideal:** se descarta hasta tener al menos save/load + UI de menú principal.

### E. Asset pack realista (Quaternius / Polyhaven / Mixamo)

**Por qué:** mencionado por el dev en Hito 23. Hoy los demos usan primitivas + pyramid/Fox/CesiumMan. Tener 3-5 props (cajas, barriles, mobiliario CC0) y 1-2 personajes humanoides amplía qué se puede mostrar.

**Coste:** bajo (selección + commit de assets + un demo con scripts) si se ciñe al pipeline actual. Medio si hay que bake nuevos materiales/normales.

**Trigger ideal:** hito de polish — buen complemento si A o B se cierra rápido.

### F. Particle system

**Por qué:** efectos visuales (fuego, humo, sparks) son la única gran feature de render que falta para tener algo visualmente "completo". Forward+ del Hito 18 ya soporta muchas point lights — añadir particles encajaría.

**Coste:** medio-alto. Diseño del sistema (CPU vs GPU, struct of arrays, shading), shader con billboards + soft particles, editor curve para params, persistencia.

**Trigger ideal:** hito de polish visual cuando el sistema base esté estable.

### G. Hot-reload de shaders ✅ ELEGIDO

**Por qué:** durante el Hito 24 perdimos varias iteraciones cuando un shader cambiado no surtía efecto al relanzar el editor (CMake los copia sólo en build). Detectar mtime + recompilar en vivo es estándar.

**Coste:** bajo. `OpenGLShader` ya tiene file paths; agregar throttle + mtime check + recompile + log de errores.

**Trigger ideal:** si se va a hacer mucho shader work (particles, post-process avanzado).

---

## Decisiones a tomar (cuando el hito se concrete)

- [ ] Cuál de los candidatos arriba se elige.
- [ ] Si el plan toca persistencia, definir si bumpea format version o si usa un campo opcional.
- [ ] Si toca scripting, definir si los nuevos features de Lua respetan el sandbox (no `io`, no `os`, no `package`).

---

## Riesgos identificados (genéricos, completar al concretar el hito)

- Cada candidato tiene riesgos propios; documentarlos al elegir.
