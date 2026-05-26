# PLAN F3H15 — Sub-fase 3.3 siguiente paso

**Estado:** **A DEFINIR** (arrancar tras F3H14).
**Predecesor:** F3H14 (mejoras del MeshThumbnailRenderer — cache disco + resolución + gradiente + mtime).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 `(F3H14 - F3H19)`.

---

## Avance de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer (cache disco + resolución + gradiente)
F3H15 –  — ⬅ próximo: a definir entre A/B/C
F3H16 –  — Hover preview ampliada (tooltip grande)
F3H17 –  — Drag&drop con feedback visual (drop zones destacadas)
F3H18 –  — Validador de assets rotos (panel)
F3H19 –  — Rename con cascada (.material/.lua/refs)
```

---

## Candidatos para F3H15

### A) Thumbnails de materiales (esfera PBR) — del plan original

`PLAN_FASE3.md` declara:
> **F3H15 — Thumbnails de materiales (esfera PBR).**
> Render off-screen de una esfera con el material aplicado + lighting estándar. Mismo cache + lazy pattern.

**Descubrimiento a confirmar:** F2H81 ya forward-decl `MaterialPreviewRenderer` en `AssetBrowserPanel.h` (línea 21) + `setMaterialPreviewRenderer` (línea 44). Probable que **ya esté implementado igual que `MeshThumbnailRenderer`** desde F2H81. Si está, F3H15 se reduce a **agregar cache disco + resolución configurable** al `MaterialPreviewRenderer` siguiendo el patrón de F3H14 (mismo helper `MeshThumbnailDiskCache` o uno hermano).

**Trabajo estimado (si MaterialPreviewRenderer ya existe completo):**
- Reusar `MeshThumbnailDiskCache` o crear `MaterialThumbnailDiskCache` espejo (mismo FNV-1a, distinto filename prefix `mat_<hash>_<size>.png`).
- `setDiskCacheRoot` en `MaterialPreviewRenderer`.
- Live recreación al cambio de `thumbnailResolution` (reusa la pref de F3H14 — un solo slider afecta meshes + materials).
- Gradiente fondo: ya existe el shader `thumbnail_bg.vert/frag`, lo reusa.

**Por qué cierra parcial Sub-fase 3.3:** segundo bloque de previews. Asset Browser ya muestra meshes con thumb (F2H80 + F3H14); materials sigue siendo lista de texto si el preview no está completo (a verificar) o regenera al boot si está pero sin cache disco.

### B) Hover preview ampliada (F3H16 adelantado)

`PLAN_FASE3.md` describe:
> **F3H16 — Hover preview ampliada.**
> Hover prolongado (> N ms, configurable) sobre asset → tooltip grande con preview ampliado + metadata. Estilo Substance Designer.

**Por qué adelantarlo:** depende de tener thumbnails listos (F3H14 cubre meshes). La preview ampliada es 256×256 o 384×384 — podemos reusar el cache disco con un size distinto (filename `_256` o `_384`), o generar on-demand cuando se gatille el hover.

**Trabajo estimado:**
- En `AssetBrowserPanel::renderMeshesTab`: detectar hover prolongado (ImGui::IsItemHovered con duración > 0.5s).
- Mostrar `ImGui::BeginTooltip` con `ImGui::Image(largeThumbId, {384, 384})` + metadata (path, dimensions, bbox, material count).
- Para el `largeThumbId`: pedir `m_thumbnails->thumbnailFor(meshId, *assets)` con un setting de override de size (necesita un parámetro nuevo) — o usar el thumb actual escalado por ImGui.

**Diferencia con A:** B es UX puro sobre los thumbs ya cacheados; A agrega cobertura a otro tipo de asset.

### C) Backlog UX descubierto en F3H12

Memoria `backlog-ux-gaps-editor`:
- Spawn de ForceField/Cloth desde el menú "+ Crear Entidad" / Hierarchy / AssetBrowser.
- Workflow "agregar sonido al mesh" (puerta con audio al activarse).

Items chicos del backlog que no rompen el orden del plan. Probablemente más rápido cerrarlos como hito chico antes de seguir con A/B.

---

## Recomendación

Yo (Claude) sugiero **opción A — Thumbnails de materiales**:
1. Sigue el orden del plan original (F3H15 dedicado).
2. Bajo riesgo si `MaterialPreviewRenderer` ya existe (F3H14 demostró que F2 dejó muchos renderers de preview ya hechos — espejo del descubrimiento de F2H80).
3. Cierra el Asset Browser visualmente — meshes ✅, materials ✅.
4. La pref `thumbnailResolution` ya existe (F3H14), no hay UI nueva en User Preferences.

**Pregunta al dev cuando arranque F3H15:**
1. ¿Confirmás opción A o querés B/C?
2. Si A: ¿unificamos `MeshThumbnailDiskCache` con la cache de materials, o files separados con prefix distinto?

---

## Lo que NO toca F3H15 (cualquiera sea la opción)

- F3H17 (Drag&drop): hito propio.
- F3H18 (Validador): hito propio.
- F3H19 (Rename con cascada): hito propio.
- Sub-fase 3.4 (Viewport pro): F3H20+.
- Async generation de thumbs (thread pool): backlog si sync molesta.
