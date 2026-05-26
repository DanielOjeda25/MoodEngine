# PLAN F3H14 — Mejoras del MeshThumbnailRenderer (cache disco + resolución + gradiente + mtime)

**Estado:** **CERRADO** (`v2.14.0-fase3-hito14`, 2026-05-26). Primer hito de Sub-fase 3.3.
**Predecesor:** F3H13 (cierre Sub-fase 3.2 — Reset to default per-field).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Thumbnails de meshes (preview 3D)".

---

## Descubrimiento al arrancar el hito

F2H80 (Fase 2) **ya implementó** `MeshThumbnailRenderer` + integración con Asset Browser + modal "+ Crear Entidad":

- `src/engine/render/preview/MeshThumbnailRenderer.{h,cpp}`: renderer PBR + IBL completo, encuadre AABB automático, cache en memoria por `meshId`, `thumbnailForPrimitive` para CSG kits.
- `AssetBrowserPanel`: ya tiene grid 80×80 en `renderMeshesTab` que pide `m_thumbnails->thumbnailFor(meshId, assets)`.
- Fondo: gris liso `(0.16, 0.16, 0.18)` ya seteado.

**Lo que F3H14 originalmente proponía (preview 3D + grid + lazy gen) ya está cubierto por F2H80.** Scope de F3H14 se reduce a las 4 mejoras puntuales sobre lo existente.

---

## Scope (4 mejoras)

### A) Cache en disco (`<proyecto>/.cache/thumbs/`)

Hoy el cache es solo en memoria — al cerrar el editor se pierde, al reabrir el editor regenera todos los thumbs (1-2s con muchos meshes; visible como flicker).

**Implementación:**
- Nueva clase `MeshThumbnailDiskCache` (`src/engine/render/preview/MeshThumbnailDiskCache.{h,cpp}`):
  - Hash FNV-1a 64 del logical path (mismo patrón que `LodCache::hashLogicalPath`).
  - Filename: `mesh_<hash>_<size>.png` (size incluido para que un cambio de resolución no chocara nombres).
  - `tryLoad(meshFsPath, cachePngPath, outRgba, outW, outH) → bool`: chequea `last_write_time(cachePath) >= last_write_time(meshPath)`; si OK lee PNG con `stbi_load`. Sino devuelve false. Sin sidecar `.meta` — el mtime del propio PNG sirve.
  - `store(cachePngPath, rgbaData, w, h)`: stbi_write_png + asegura `create_directories(parent_path)`.
- `MeshThumbnailRenderer`:
  - `setDiskCacheRoot(std::filesystem::path)`: setter inyectado por `EditorApplication` al cargar proyecto.
  - En `thumbnailFor(meshId, assets)`:
    1. Cache memoria HIT → return.
    2. Sino, si diskCacheRoot set → `tryLoad(meshFsPath, cachePath, ...)`. Si OK → crear `OpenGLFramebuffer` + `glTexImage2D(rgba)` → cachear memoria → return.
    3. Sino render PBR original → `glReadPixels` + `stbi_write_png` al cache → cachear memoria → return.
- `EditorApplication`:
  - Al `loadProjectFromPath`: `m_meshThumbnails->setDiskCacheRoot(m_project->root / ".cache" / "thumbs")`.
  - Al cerrar proyecto: setter con `fs::path{}` (cache disco off — fallback a solo memoria).

### B) Resolución configurable (`UserSettings.editor.thumbnailResolution`)

Hoy hardcoded 128 en el constructor.

**Implementación:**
- `UserSettings::EditorSettings` agrega `int thumbnailResolution = 128;` (clamp `[64, 512]`).
- `editorSettingsToJson/fromJson`: subkey `"thumbnail_resolution"` solo si != default.
- `UserPreferencesPanel.cpp` `drawEditorTab`: SliderInt 64-512 con reset button + tooltip i18n.
- `EditorApplication::tick()` (o equivalente): detecta cambio vs `m_lastThumbnailResolution`; si cambió → `m_meshThumbnails = std::make_unique<MeshThumbnailRenderer>(newSize)` + reinyectar IBL + reinyectar diskCacheRoot + reinyectar al AssetBrowser.

### C) Mtime invalidation

Hoy el cache memoria nunca expira (vive lo que vive el renderer); el cache disco que añadimos en (A) ya tiene check `last_write_time(cache) >= last_write_time(mesh)`. Para que el cache memoria también respete cambios:

**Implementación:**
- `MeshThumbnailRenderer::thumbnailFor`:
  - Si memoria HIT pero `m_cachedMtimes[meshId] < currentMtime(meshFsPath)` → invalidar memoria + recargar via disco/render.
- Mantenemos un `std::unordered_map<u32, fs::file_time_type> m_cachedMtimes` paralelo al cache de FBOs.

### D) Gradiente vertical en el fondo

Hoy fondo gris liso `(0.16, 0.16, 0.18)`. El dev pidió "gris medio con leve gradiente" (estilo Substance/Marmoset).

**Implementación:**
- Shader nuevo `shaders/thumbnail_bg.vert/frag`:
  - Vert: fullscreen triangle trick (sin VBO, usa `gl_VertexID`).
  - Frag: gradient vertical entre `(0.13, 0.13, 0.15)` (abajo) y `(0.20, 0.20, 0.22)` (arriba), por `gl_FragCoord.y / uViewportSize.y`.
- `MeshThumbnailRenderer::clearAndSetGlState`:
  - glClearColor + glClear como ahora.
  - Bind shader bg + glDrawArrays(GL_TRIANGLES, 0, 3) con depth test off (escribe color, no depth).
  - Reenable depth test para el mesh.
- Cargar shader en el constructor del renderer (mismo patrón que `m_pbrShader`).

---

## Decisiones

1. **Filename incluye `_<size>`**: cambio de resolución NO invalida cache disco vieja (otros sizes quedan por si el dev vuelve). Simple coexistencia. Limpieza manual o backlog.
2. **Sin sidecar `.meta`**: el mtime del propio PNG basta para validar contra mtime del mesh. Menos archivos, menos race conditions.
3. **Recrear renderer al cambiar resolución vs setter**: recrear es más simple (FBOs lazy van con el size del renderer). El cache memoria se pierde pero el cache disco persiste (nuevos thumbs a 64, viejos PNGs a 128 ignorados).
4. **`<proyecto>/.cache/thumbs/` vs `assets/.cache/thumbs/`**: el dev pidió `<proyecto>/.cache/` (raíz del proyecto, no dentro de `assets/`). Honramos: cache fuera de los assets, fácil de gitignore con una sola línea `.cache/`.

---

## Lo que NO toca F3H14

- F3H15 (Material thumbs): hito propio.
- Comando "Clear thumbnails" en menu Debug: backlog si el dev lo pide.
- Hot-reload de meshes (assimp re-import al cambio del archivo): scope distinto, no es del Asset Browser.
- Async generation (thread pool): scope futuro si el sync molesta con 100+ meshes.
- Limpieza automática de PNGs huérfanos al cambiar `_<size>`: backlog.
