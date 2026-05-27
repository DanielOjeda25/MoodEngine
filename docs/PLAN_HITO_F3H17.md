# PLAN F3H17 — Drag & drop con feedback visual

**Estado:** ✅ **CERRADO** (`v2.17.0-fase3-hito17`, 2026-05-26).
**Predecesor:** F3H16 (hover preview ampliada del Asset Browser).
**Origen:** `PLAN_FASE3.md` Sub-fase 3.3 lista "Drag & drop con feedback visual".

---

## Avance de Sub-fase 3.3

```
F3H14 ✅ — Mejoras MeshThumbnailRenderer
F3H15 ✅ — Mejoras MaterialPreviewRenderer
F3H16 ✅ — Hover preview ampliada
F3H17 ✅ — Drag & drop con feedback visual
F3H18 –  — ⬅ próximo: Validador de assets rotos
F3H19 –  — Rename con cascada
```

---

## Lo que entregó

### Helper compartido `DragDropFeedback`

Nuevo header `src/editor/ui/DragDropFeedback.h` con utilidades reusables por viewports + Inspector slots:

- `isViewportDragActive()` — true si hay drag activo de cualquier tipo soportado por el viewport (`MOOD_TEXTURE_ASSET` / `MESH` / `PREFAB` / `MATERIAL` / `SCRIPT` / `ITEM` / `VEHICLE`).
- `drawDropHalo(drawList, min, max, cursorOver, thickness=3.0f)` — pinta borde cyan (`IM_COL32(80, 180, 255, 200)`) si el cursor NO está sobre el target, verde (`IM_COL32(80, 230, 130, 230)`) si SÍ. Llamar SOLO cuando hay drag activo.
- `isDragActiveOfType(type)` — single check para un payload específico. Re-export del helper que vivía en `InspectorPanel_Internal.h:139` para que los Inspector slots no tengan que incluir el Internal del Inspector.
- `drawItemDropHalo(cursorOver)` — wrapper que pinta el halo alrededor del último ítem dibujado (`GetItemRectMin/Max`), thickness 2 px para slots chicos del Inspector.
- `cancelDragOnEscape()` — cancela el drag activo cuando el dev presiona Esc. Accede a `ImGuiContext` interno (`<imgui_internal.h>` — la API pública no expone cancel) y limpia `DragDropActive` + `DragDropPayload` + `DragDropAcceptId*`.

### Highlight 3D sobre el target (overlay 3D del editor)

Reescritura de la sección de drag highlights en `EditorRenderPass_Overlay.cpp`. Antes había **dos colores y dos shapes**: cubo cyan sobre tile (Texture/Mesh/Prefab) y OBB amarillo sobre entity (Material/Script). F3H17 **unifica el lenguaje visual a AABB cyan brillante** (`vec3(0.30, 0.85, 1.0)`) para todos los drag targets y **extiende Texture a iluminar el Brush bajo cursor** (el handler real prioriza brush; sin highlight el dev no veía dónde caería).

Flow:
1. Paso 1: si el drag puede apuntar a una entity (`Texture` / `Material` / `Script`) y el cursor está sobre el viewport, hacer `pickEntity` y dibujar `AABB cyan brillante` del **Brush** (Texture/Material/Script) o **MeshRenderer** (Material/Script) bajo el cursor. Usa los helpers públicos `brushAabbWorld` / `meshAabbWorld` de `ScenePick.h`.
2. Paso 2: si NO se pintó entity en este frame y hay tile pick válido + el drag es tile-target (`Texture` / `Mesh` / `Prefab`), pintar cubo cyan sobre el tile. Evita el doble feedback cuando el dev apunta a un brush que casualmente se solapa con un tile.

El helper local de 12-líneas-manualmente-armadas para el OBB se reemplaza por `dbg.drawAabb(world_aabb, color)` (más conciso, mismo resultado).

### Inspector slots con halo (drop targets fuera del viewport)

6 paneles del Inspector + el Material Editor ganan halo overlay (cyan/verde) cuando hay drag activo de un tipo compatible con ese slot:

- `InspectorPanel_MeshRenderer.cpp` — slots de texture (albedo/normal/roughness/metallic/ao/emissive) iluminan en cyan ante `MOOD_TEXTURE_ASSET`.
- `InspectorPanel_Animation.cpp` — slots de animation clip ante `MOOD_ANIMCLIP_ASSET`.
- `InspectorPanel_Vehicle.cpp` — slot de configPath ante `MOOD_VEHICLE_ASSET`.
- `InspectorPanel_Joint.cpp` — slots de entity reference ante `MOOD_ENTITY` (drag desde Hierarchy).
- `InspectorPanel_Inventory.cpp` — slots de item ante `MOOD_ITEM_ASSET`.
- `MaterialEditorPanel.cpp` — cada texture slot del material ante `MOOD_TEXTURE_ASSET`.

Llamado tras el widget drop target: `if (DragDropFeedback::isDragActiveOfType("MOOD_X")) DragDropFeedback::drawItemDropHalo(ImGui::IsItemHovered());`. Halo verde si cursor sobre el slot (drop OK), cyan si payload activo pero cursor en otro lado (drop posible al mover el cursor).

### Cancel con Esc

`EditorApplication::beginFrame()` llama `DragDropFeedback::cancelDragOnEscape()` tras `ImGui::NewFrame()`. Si hay drag activo y el dev presiona Esc, se limpia el state interno de ImGui y el frame siguiente ningún target acepta el payload (al soltar el mouse, ImGui hace cleanup residual normal).

### Bug fix: mesh thumbs invertidos (regresión introducida por F3H14/F3H15)

Durante validación visual el dev reportó que TODOS los meshes del Asset Browser se veían **upside-down**. Root cause: `AssetThumbnailDiskCache::tryLoad` llama `stbi_load`, y el flag global `stbi_set_flip_vertically_on_load(true)` lo setea `OpenGLTexture.cpp:64` de forma persistente — entonces stb YA flippea los bytes a convention GL. El código en `MeshThumbnailRenderer.cpp` + `MaterialPreviewRenderer.cpp` hacía un **flip manual adicional** después de cargar el PNG, resultando en **doble flip** → upload a GL con orientación invertida → ImGui::Image con `uv(0,1)-(1,0)` mostraba upside-down.

Fix (3 archivos):
- `AssetThumbnailDiskCache.cpp`: agregar `stbi_set_flip_vertically_on_load(true);` explícito antes del `stbi_load` para garantizar la convention GL aunque algún load previo lo hubiese dejado en false.
- `MeshThumbnailRenderer.cpp` (HIT path): eliminar el flip manual; uploadear `rgba` directo al FBO color texture.
- `MaterialPreviewRenderer.cpp` (HIT path): mismo cambio.

Los PNGs en disco quedan correctos (top-down, escritos con `glReadPixels + flip + stbi_write_png`); el bug afectaba solo la lógica de carga. No hay que borrar los caches existentes — con el fix los mismos PNGs se muestran correctamente.

### Polish post-validación: halo de viewport movido del borde al target

Primer intento de F3H17 (pre-validación): además del highlight 3D existente, agregar un halo cyan/verde sobre el **borde del panel viewport** cuando hay drag activo (`drawDropHalo` llamado desde `ViewportPanel.cpp` tras `EndDragDropTarget`). El dev reportó visualmente: *"el halo cyan no debería aparecer sobre el área que afectaré, me refiero si arrastro una textura y la idea es que un plano tome esa textura no debería ese plano tener el halo cyan?"*.

Refactor: **eliminar el halo del borde** (era distractor — el dev no sabía que el highlight 3D ya estaba pintando el target) y **reescribir el highlight 3D** con lenguaje visual unificado (cyan brillante para todo drag target, AABB en lugar del OBB amarillo, extensión a Texture sobre Brush). El halo de los Inspector slots se mantiene — ahí sí tiene sentido (los slots no son obvios sin la pista visual).

---

## Sites tocados

```
src/editor/ui/DragDropFeedback.h                          NEW
src/editor/application/EditorApplication.cpp              cancelDragOnEscape() en beginFrame
src/editor/application/EditorRenderPass_Overlay.cpp       drag highlight reescrito
src/editor/panels/scene/ViewportPanel.cpp                 halo del borde removido
src/editor/panels/scene/InspectorPanel_MeshRenderer.cpp   halo en texture slots
src/editor/panels/scene/InspectorPanel_Animation.cpp      halo en clip slot
src/editor/panels/scene/InspectorPanel_Vehicle.cpp        halo en config slot
src/editor/panels/scene/InspectorPanel_Joint.cpp          halo en entity slots
src/editor/panels/scene/InspectorPanel_Inventory.cpp      halo en item slot
src/editor/panels/assets/MaterialEditorPanel.cpp          halo en texture slots
src/engine/render/preview/AssetThumbnailDiskCache.cpp     stbi_set_flip_vertically_on_load explicito (fix doble flip)
src/engine/render/preview/MeshThumbnailRenderer.cpp       HIT path sin flip manual (fix doble flip)
src/engine/render/preview/MaterialPreviewRenderer.cpp     HIT path sin flip manual (fix doble flip)
```

---

## Decisiones

**D1 — Helper compartido `DragDropFeedback` vs duplicar en cada panel.**
Halo lookup + draw + Esc cancel son los mismos 3-4 helpers usados en 8 sites (viewport + 5 Inspector panels + Material Editor + viewport border refactored). Header de 100 LOC, sin .cpp. Si futuro hito agrega más drop targets, suman `drawItemDropHalo()` y listo.

**D2 — Cancel con Esc vía API interna de ImGui.**
ImGui no expone API pública para cancelar drag (es decisión de su roadmap — argumentan que el dev puede soltar el botón). Necesitamos cancel porque Esc es la convención universal (Photoshop / Blender / Unity / VS Code). Usar `<imgui_internal.h>` + `GImGui->DragDropActive = false` es estable suficiente — el field ha estado ahí desde la inception del drag&drop module en 2019.

**D3 — Halo del borde del viewport eliminado tras feedback del dev.**
Pre-validación pensé que doble feedback (borde + target 3D) era complementario. El dev fue claro: el target SÍ debe iluminarse, el borde NO (era distractor que tapaba la atención del feedback real). Decisión registrada en commit: el lenguaje visual del editor es "el target específico se ilumina", no "el panel acepta".

**D4 — Lenguaje visual unificado a cyan brillante para todos los drag targets.**
Pre-F3H17 el editor tenía dos convenciones diferentes: cubo cyan para tiles (Texture/Mesh/Prefab) y OBB amarillo para entities (Material/Script). Convergir a **AABB cyan brillante** (vec3(0.30, 0.85, 1.0)) en ambos casos — mismo color del halo de Inspector slots, mismo lenguaje en todo el editor. El amarillo del OBB se elimina (era residual de cuando el highlight era visualmente distinto del cyan del tile).

**D5 — Texture drag highlight extendido a Brush, NO a MeshRenderer suelto.**
El handler real (`processViewportTextureDrop`) prioriza Brush > Tile; mesh entities sueltas con MeshRenderer caen al tile pick. Si pintáramos highlight de mesh entity al hacer drag de texture, el dev soltaría esperando que se asigne y el handler no asignaría (caería al tile debajo). Mantener consistencia visual ↔ behavior: highlight solo lo que realmente recibe.

---

## Validación

Dev confirmó "todo ok" tras testear:
1. Meshes en Asset Browser se ven derechos (no upside-down) — fix del doble flip.
2. Drag de textura sobre brush → AABB cyan brillante sobre el brush.
3. Drag de textura sobre tile vacío → cubo cyan sobre el tile.
4. Drag de material/script sobre mesh entity (zorro, cube) → AABB cyan brillante.
5. Halo cyan del borde del panel viewport ya no aparece — el feedback ahora está en el target específico.
6. Inspector slots con drag activo compatible → halo overlay cyan/verde.
7. Esc durante drag cancela inmediatamente.

---

## Lo que NO toca F3H17

- F3H18 (Validador de assets rotos): hito propio.
- F3H19 (Rename con cascada): hito propio.
- Sub-fase 3.4 (Viewport pro): F3H20+.
- Highlight de Hierarchy entries durante drag de entity: el Hierarchy actualmente no es drop target (solo emite `MOOD_ENTITY`); skip explícito.
- Cursor custom (ghost del thumb más grande, "+" verde / "X" rojo): el cursor de ImGui ya muestra el contenido del `BeginDragDropSource`, no agregamos más; futura iter si emerge fricción.
