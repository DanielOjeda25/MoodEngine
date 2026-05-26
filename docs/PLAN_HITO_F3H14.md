# PLAN F3H14 — Arranque Sub-fase 3.3 (Asset Browser de verdad)

**Estado:** **A DEFINIR** (arrancar tras cierre F3H13).
**Predecesor:** F3H13 (Reset to default per-field — cierre Sub-fase 3.2).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 `(F3H14 - F3H19)`.

---

## Norte de Sub-fase 3.3

> *"Asset Browser de verdad"* — pasa de listado de paths a panel visual con previews, drag&drop con feedback, validador de assets rotos, rename con cascada.

Hitos planeados (PLAN_FASE3 §4):
1. **F3H14 — Thumbnails de meshes** (preview 3D off-screen + cache disco).
2. **F3H15 — Thumbnails de materiales** (esfera PBR con material aplicado).
3. **F3H16 — Hover preview ampliada** (tooltip grande estilo Substance).
4. **F3H17 — Drag&drop con feedback visual** (drop zones destacadas).
5. **F3H18 — Validador de assets rotos** (panel dedicado).
6. **F3H19 — Rename con cascada** (refs en `.material`/`.moodmap`/`ScriptComponent`).

---

## Candidato F3H14 (recomendado del plan original)

### Thumbnails de meshes (preview 3D)

`PLAN_FASE3.md` declara:
> **F3H14 — Thumbnails de meshes (preview 3D).**
> Render off-screen de cada `.moodmesh` cargado. Cache en `<proyecto>/.cache/thumbs/`. Lazy generation. Resolución configurable en Preferences.

**Por qué es el primer hito de 3.3:**
- Es **prerequisito visual** de F3H15-F3H17 (todos los thumbs y previews necesitan el patrón render off-screen + cache).
- Establece el **directorio `.cache/thumbs/`** y el formato (PNG con hash del asset) — F3H15+ lo reusan.
- Establece la **regla de invalidación** (cuando el mesh cambia, invalidar el thumb).
- El Asset Browser actual muestra paths como texto — un dev escaneando 50 meshes para "el sniper rifle" pierde tiempo. Con thumbs es identificación instantánea.

### Trabajo estimado

1. **Off-screen rendering**: FBO mínimo (color + depth) en `editor/thumbnails/MeshThumbnailRenderer.cpp`. Reusa `SceneRenderer` light path con 1 luz direccional + ambient. Modelo centrado/escalado a frame.
2. **Cache disco**: `<proyecto>/.cache/thumbs/mesh_<hash>.png`. Hash = sha1 del path + mtime (invalida si el `.moodmesh` cambia). Lazy: si existe el png + hash matches → load; sino → render off-screen + write.
3. **Cache memoria**: `AssetThumbnailCache` LRU con N slots (texture handle + path → handle). Asset Browser pide `getThumbnail(meshPath)` → cache HIT devuelve handle, MISS dispara render async/sync (sync v1, async F3H15+ si la latencia molesta).
4. **UI del Asset Browser**: grid con tiles 64×64 / 96×96 / 128×128 (toggle en topbar del browser, default 96). Tile = imgui Image + label debajo. Hover muestra tooltip con path completo.
5. **Resolución configurable** (UserSettings > Editor): `thumbnailResolution` int slider 64-256, default 128. Recalc-on-demand al cambiar.
6. **Invalidación**: hook al save del `.moodmesh` (cuando exista import re-bake) o al mtime mismatch al startup del editor. v1: validar mtime al abrir el browser.

### Decisiones a tomar al arrancar

- **Sync vs async**: v1 sync (mismo frame que el browser pide). Si la lib de assets crece, mover a thread pool. Empezar simple.
- **Cache path**: ¿`<proyecto>/.cache/thumbs/` (per-proyecto, .gitignore-able) o `<APPDATA>/MoodEngine/thumbs/<proyecto_hash>/` (per-instalación)? Per-proyecto es más obvio (el dev ve la carpeta) y se borra con el repo limpio.
- **Default lighting**: ¿luz fija pre-calibrada (mismo lighting para todos los thumbs — consistencia visual) o usar lighting de la escena actual? Pre-calibrada (consistencia + thumbs portables entre escenas).
- **Background**: ¿checkerboard transparente (DCC standard), gris liso, o el skybox actual de la escena? Gris medio con leve gradiente — neutral, no compite con el mesh.

### Trabajo NO trivial

- **Skeletal meshes**: ¿se rendean en T-pose (bind pose) o estáticos? T-pose es el default sensato.
- **Meshes muy chicos / muy grandes**: el frame debe encuadrar correctamente. Bounding box → escalado uniforme a "encajar en cámara".
- **Meshes con materials missing**: usar material default (rosado debug) o el material asignado al primer load? Material asignado — el thumb refleja el estado real.

---

## Alternativas candidatos para F3H14 (a discutir con el dev)

### B) Sub-fase 3.3 mecánica de juego (no Asset Browser visual)

Si el dev prefiere mecánicas observables en el juego antes que pulido del editor:
- Saltar a Sub-fase 3.4 (Viewport pro) directamente.
- F3H14 = snapping configurable (F3H20 del plan original — toolbar de snap más rica, vertex/face/angle).

Trade-off: rompe el orden del plan; Asset Browser sin thumbs sigue siendo "lista de paths".

### C) Backlog UX descubierto en F3H12

Memoria `backlog-ux-gaps-editor`:
- Spawn de ForceField/Cloth desde el menú Add Entity / Hierarchy / AssetBrowser.
- Workflow "agregar sonido al mesh" (puerta con audio al activarse) — Inspector slot, evento o prefab.

Pequeños, pero rompen el flujo de Sub-fase 3.3. Mejor anotarlos como tareas chicas y procesarlas al cerrar 3.3 si no aparecieron por el camino.

---

## Recomendación

Yo (Claude) sugiero **opción A — Thumbnails de meshes**:
1. Es el primer hito del plan original de Sub-fase 3.3.
2. Habilita F3H15+ (mismo patrón de render + cache para materials).
3. Mejora inmediata visible en el editor — el dev "siente" el panel cambiar.
4. Scope acotado (off-screen render + cache disco + UI grid).

**Pregunta al dev cuando arranque F3H14:** ¿confirmás opción A o querés B/C?

---

## Lo que NO toca F3H14 (cualquiera sea la opción)

- F3H15 (Material thumbs): hito propio para no inflar scope.
- F3H16 (Hover ampliada): depende de F3H14/F3H15 estando.
- F3H17 (Drag&drop): scope distinto (eventos drag, no thumbs).
- F3H18-F3H19 (Validador + Rename): hitos propios.
- Sub-fase 3.4 (Viewport pro + Performance): F3H20-F3H27.
