# PLAN F3H15 — Mejoras del MaterialPreviewRenderer (cache disco + resolución + gradiente compartido)

**Estado:** **CERRADO** (`v2.15.0-fase3-hito15`, 2026-05-26). Segundo hito de Sub-fase 3.3.
**Predecesor:** F3H14 (mejoras del MeshThumbnailRenderer).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Thumbnails de materiales (esfera PBR)".

---

## Descubrimiento al arrancar el hito

F2H21 + F2H81 (Fase 2) **ya implementaron** `MaterialPreviewRenderer` completo:
- `src/engine/render/preview/MaterialPreviewRenderer.{h,cpp}`: renderer esfera PBR + IBL, FBO 256×256.
- `thumbnail(materialId)`: cache memoria por `materialId`, lazy generation (igual patrón que `MeshThumbnailRenderer::thumbnailFor`).
- `renderPreview(mat)`: render animado (rotación) para el Material Editor panel.
- Integración con `AssetBrowserPanel` via `setMaterialPreviewRenderer` desde F2H81.

Mismo escenario que F3H14: el render está; faltan las 4 mejoras de cache disco + pref + gradient + mtime.

---

## Scope (mismo paquete que F3H14, ahora compartido)

### A) Cache disco compartido con meshes

Generalización del helper de F3H14:

- **Renombrar** `MeshThumbnailDiskCache` → `AssetThumbnailDiskCache`. Mismo namespace, mismo algoritmo, agregar parámetro `prefix` en `pathFor`. Filename: `<prefix>_<hash>_<size>.png`. Prefix `"mesh"` para meshes, `"mat"` para materiales — discriminan en el mismo directorio `<proyecto>/.cache/thumbs/`.
- `MaterialPreviewRenderer::thumbnail(materialId)`:
  - Cache memoria HIT → return como antes.
  - Sino: si `m_diskCacheRoot` set → `tryLoad` con `assets.resolvePath(materialPathOf(id))` como source mtime. HIT → crear FBO + `glTexSubImage2D` el RGBA al color attachment + flip vertical (PNG↔GL). Cache memoria + return.
  - Sino: render path original → `glReadPixels` + flip + `stbi_write_png`. Cache memoria + return.
- `setDiskCacheRoot(fs::path)`: setter inyectado por `EditorApplication` igual que F3H14.

### B) Resolución compartida (`UserSettings.editor.thumbnailResolution`)

**Decisión:** una sola pref afecta meshes + materiales (un solo slider en User Preferences, consistencia UX).

- `EditorApplication_Init.cpp`: construcción inicial del `MaterialPreviewRenderer` usa `UserSettings::editor().thumbnailResolution` en lugar de `256u` hardcoded.
- `EditorApplication_Run.cpp`: el bloque de live recreate de F3H14 se amplía — al cambiar la pref, recrear AMBOS renderers (`m_meshThumbnails` + `m_materialPreview`) + reinyectar IBL + diskCacheRoot + Asset Browser + Material Editor.

### C) Gradient vertical en el fondo

Reusar el shader `shaders/thumbnail_bg.vert/frag` que F3H14 ya creó (sin duplicar archivos).

- `MaterialPreviewRenderer` constructor: cargar `m_bgShader` + crear `m_dummyVao` (igual que F3H14).
- En `renderSphereToBoundFbo`: después del `glClear`, draw del fullscreen triangle con `m_bgShader`.

### D) Mtime invalidation

Cubierta por (A) — el `tryLoad` chequea `last_write_time(cachePng) >= last_write_time(materialFsPath)`. Si el dev edita un `.material` con el Material Editor (o externamente), el siguiente boot regenera ese thumb.

---

## Decisiones

1. **Helper compartido `AssetThumbnailDiskCache` vs caches separadas**: un solo helper con `prefix` parameter. Razón: el algoritmo es idéntico (FNV-1a + filename + mtime + stbi_load/write). Duplicar = drift entre los dos paths cuando alguno se mejore. **Trade-off**: el helper "sabe" de un detalle (prefix) que es del caller, pero a cambio cualquier asset futuro (audio thumbs, animation thumbs si se quisieran) reusa con un nuevo prefix.

2. **Una sola pref `thumbnailResolution`**: un único slider afecta meshes + materiales. Razón: UX consistencia (el dev no piensa "esto es mesh, esto es mat — cada uno con su resolución"). **Trade-off**: si alguna vez se quiere asimetría (meshes a 96, materiales a 256), agregar la 2da pref después. Por ahora no hay caso de uso real.

3. **Shader `thumbnail_bg` compartido entre renderers**: un solo `.vert + .frag` en `shaders/`. Los 2 renderers lo cargan independientemente (cada uno tiene su `m_bgShader` instance — la API de `OpenGLShader` es ownership por instancia). Razón: el shader es trivial (un vert + un frag), no justifica un singleton.

4. **Filename con prefix `mat_` en disco**: `mat_<hash>_<size>.png` (no `material_` para keep it short). Coexiste con `mesh_<hash>_<size>.png` en el mismo directorio sin colisión. Razón: filenames más cortos = listing más legible al inspeccionar manualmente.

---

## Lo que NO toca F3H15

- F3H16 (Hover preview ampliada): hito propio.
- F3H17 (Drag&drop con feedback visual): hito propio.
- F3H18 (Validador de assets rotos): hito propio.
- F3H19 (Rename con cascada): hito propio.
- Async thumb generation (thread pool): backlog si la latencia sync molesta con cientos de meshes/materiales.
- Limpieza automática de PNGs huérfanos al cambiar resolución repetidas veces: backlog (hoy quedan en disco hasta limpieza manual; el directorio crece).
